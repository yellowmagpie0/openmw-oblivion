# Local development handbook

This is the fork-local handoff for the content-inspection and authoring tools
ported from `/home/maciek/openmwdev`. Read it before extending the MCP server or
starting substantial content work. The detailed tool reference lives in
`apps/opencs/README-MCP.md`; `apps/opencs/FARGOTH-QUEST.md` is the worked TES3
inspection example.

## Imported baseline and scope

The `openmw-cs-mcp` implementation was developed in two local commits based on
OpenMW commit `0e7df0d95b`:

- `d4a36ce93a`: quest inspection and patch writing;
- `3e325bb109`: environment inspection, CELL/LAND/NPC/item authoring, and world
  validation (the source release identified itself as version 0.3.0).

Those changes were applied to this fork without importing the example mods,
generated ESP files, screenshots, or Morrowind-specific release records. The
examples remain available in `/home/maciek/openmwdev/mods` as reference, while
the reusable source and documentation now live in this repository.

Port verification on 2026-08-02, against fork commit `f38591bc73`:

- the focused CMake configuration below completed;
- `openmw-cs-mcp` compiled and linked against this fork's current components;
- MCP initialization negotiated protocol `2025-11-25` and exposed all eleven
  expected tools;
- `get_load_order` resolved the three Morrowind masters plus `Reedwake.esp`;
- `get_area_quests({area:"Reedwake",scope:"all"})` rediscovered seven quests;
- strict validation of the existing Reedwake fixture checked 65 sources with
  zero compiler errors/warnings and reported valid world data with no missing
  bases, misplaced exterior references, or LAND seam errors.

Important boundary: this server is an ESM3/TES3 tool. It reads and writes
Morrowind-style DIAL, INFO, SCPT, NPC_, MISC, CELL, and LAND records and places
existing ESM3 assets. The repository name and any wider Oblivion work in this
fork do not make it a TES4 authoring tool. Treat TES4 support as a future
extension requiring its own schemas, readers/writers, validation, and tests.

## Repository map

- `apps/opencs/mcpserver.cpp`: stdio JSON-RPC server, ESM3 index, inspection
  tools, ESP writer, compiler integration, and world validation.
- `apps/opencs/README-MCP.md`: concise build, registration, tools, and schema
  overview.
- `apps/opencs/FARGOTH-QUEST.md`: a reproducible graph traversal through two
  connected vanilla quests.
- `apps/opencs/CMakeLists.txt`: the `openmw-cs-mcp` target.

The source workspace used to develop the tool also contains a complete example
in `/home/maciek/openmwdev/mods`: declarative JSON sources, generated ESPs,
structural/semantic/runtime validation notes, and retained screenshots. Do not
copy those game-content artifacts into this engine fork unless a later task
specifically needs fixtures.

## Build

The target requires OpenMW-CS:

```sh
cmake -S . -B build -DBUILD_OPENCS=ON
cmake --build build --target openmw-cs-mcp -j2
```

On the current workstation the system Bullet package is single precision while
OpenMW requires double precision. CMake 4 also rejects the bundled Bullet's old
minimum-policy declaration unless a compatibility floor is supplied. A known
working focused configuration is:

```sh
cmake -S . -B build \
  -DBUILD_OPENCS=ON \
  -DBUILD_OPENMW=OFF \
  -DBUILD_OPENMW_LAUNCHER=OFF \
  -DBUILD_WIZARD=OFF \
  -DBUILD_ESSIMPORTER=OFF \
  -DBUILD_BSATOOL=OFF \
  -DBUILD_ESMTOOL=OFF \
  -DBUILD_NIFTEST=OFF \
  -DBUILD_BULLETOBJECTTOOL=OFF \
  -DOPENMW_USE_SYSTEM_BULLET=OFF \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build build --target openmw-cs-mcp -j2
```

The server writes only JSON-RPC to stdout. Load diagnostics belong on stderr;
ordinary debug output on stdout corrupts the protocol.

## Configuration discipline

Use separate configurations for writing and inspecting/running a generated
plugin.

An authoring config contains only the intended masters. For a Morrowind plugin
this normally means:

```text
replace=config
resources="/absolute/path/to/build/resources"
data="/absolute/path/to/Morrowind/Data Files"
data="/absolute/path/to/local/mods"
fallback-archive=Morrowind.bsa
fallback-archive=Tribunal.bsa
fallback-archive=Bloodmoon.bsa
content=Morrowind.esm
content=Tribunal.esm
content=Bloodmoon.esm
encoding=win1252
```

Do not load the plugin that is being regenerated. The writer declares active
content files as masters, and add operations must not collide with records from
an earlier copy of the same plugin.

An inspection/runtime config loads the completed plugin last. Always call
`get_load_order` when results are surprising.

Command-line detail:

- `openmw-cs-mcp --config` takes the path to `openmw.cfg`;
- `openmw --config` takes the directory containing `openmw.cfg`.

## Calling the MCP directly

For one-shot audits, send `initialize`, `notifications/initialized`, and the
tool call in that order:

```sh
jq -nc '
  {jsonrpc:"2.0",id:1,method:"initialize",params:{
    protocolVersion:"2025-11-25",capabilities:{},
    clientInfo:{name:"local-audit",version:"1"}
  }},
  {jsonrpc:"2.0",method:"notifications/initialized",params:{}},
  {jsonrpc:"2.0",id:2,method:"tools/call",params:{
    name:"get_load_order",arguments:{}
  }}
' | ./build/openmw-cs-mcp --config /absolute/path/to/openmw.cfg
```

For large results, redirect stdout to JSONL. Tool payloads are JSON encoded in
`result.content[0].text`:

```sh
jq -r 'select(.id==2) | .result.content[0].text' result.jsonl | jq .
```

Use `tools/list` rather than guessing the live schema. Keep a process alive for
interactive work; a one-shot process reloads all masters each time.

## Tool inventory and inspection order

Read-only tools:

- `get_load_order`: effective content order and resolved paths;
- `find_quests`, `get_quest`, and `get_area_quests`: journal discovery and
  directly connected dialogue/scripts;
- `find_environments` and `get_environment`: cells, references, terrain,
  bounds, doors, destinations, NPCs, and terrain-relative placement;
- `search_records` and `get_record`: broad discovery followed by exact record
  inspection;
- `validate_plugin`: reopen an ESP, compile sources, and validate its placed
  world.

Write tools:

- `write_quest_patch`: dialogue, script, NPC, MISC, and related atomic edits;
- `write_world_plugin`: the same machinery exposed for complete ESM3 world and
  quest plugins.

For an unfamiliar quest, traverse the graph:

1. Discover it with `get_area_quests(..., "all")` or `find_quests`.
2. Open the journal and direct transitions with `get_quest`.
3. Search discovered actor, item, container, activator, and script IDs.
4. Inspect exact records with `get_record`.
5. Inspect actual placements with `get_environment`.

A journal alone is not a quest. TES3 quest behavior is distributed among
journal DIAL/INFO records, ordinary topics and greetings, attached scripts,
inventories, and CELL placements.

## Manifest workflow and atomic writing

Keep a JSON object matching the write-tool argument schema as the canonical
editable source. Load large manifests with `jq --slurpfile` rather than placing
them in a shell argument:

```sh
jq -nc --slurpfile args manifest.json '
  {jsonrpc:"2.0",id:1,method:"initialize",params:{
    protocolVersion:"2025-11-25",capabilities:{},
    clientInfo:{name:"plugin-builder",version:"1"}
  }},
  {jsonrpc:"2.0",method:"notifications/initialized",params:{}},
  {jsonrpc:"2.0",id:2,method:"tools/call",params:{
    name:"write_world_plugin",arguments:$args[0]
  }}
' | ./build/openmw-cs-mcp --config /path/to/authoring-openmw.cfg \
    > /tmp/plugin-build.jsonl
```

Use an absolute `output_path`. Set `overwrite:true` only for the intended
artifact. The writer uses a temporary file, reopens it structurally, and
atomically replaces the target through an `.openmw-cs-mcp-backup`. If an
interrupted write leaves a backup, inspect it before moving it; the next
overwrite intentionally refuses to proceed while it exists.

The write response's `validated:true` means TES3 structure/readability only. It
does not mean scripts compiled or gameplay worked.

## Current authoring limits

Supported edit operations are `add_dialogue`, `update_info`, `add_info`,
`delete_info`, `add_script`, `update_script`, `add_npc`, `add_misc`, `add_land`,
`update_land`, `add_cell`, and `update_cell`.

Existing master assets of other types can be placed, but the writer cannot yet
create arbitrary DOOR, ACTI, STAT, CONT, CREA, WEAP, ARMO, CLOT, BOOK, SPEL,
ENCH, mesh, or texture records. `add_misc` inherits model and icon from its
template. `update_cell` appends references; it has no reference delete/move API,
and placed references do not receive custom reference IDs.

If future work exceeds these constraints, extend and test the writer instead of
hand-editing binary plugins.

## Authoring rules that survived runtime testing

Dialogue INFO order is behavior: specific conditions must precede generic
fallbacks. When inserting into vanilla dialogue, anchor against inspected
neighbors. Teach new topics explicitly with `AddTopic`.

A robust choice sequence places choice-result INFOs before the prompt INFO and
repeats the relevant state guards. Failed purchases or unmet prerequisites must
leave the state retryable. Guard rewards by an exact active stage or one-time
item state to prevent repeat payouts.

Use complete source-only scripts and compile them with `validate_plugin`.
One-time guards are essential for `OnPCAdd` and `OnDeath`. After cloning an NPC,
explicitly clear an unwanted inherited script. Preserve template inventory when
it supplies clothing, choose a matching race/sex template, and visually inspect
the result.

Exterior cells are 8192 world units square. CELL `(x,y)` owns positions where
`x*8192 <= world_x < (x+1)*8192` and likewise for y. LAND uses a 65 by 65
height grid (4225 values). Multi-cell generated terrain must share identical
world-space generation parameters, and validation must confirm every shared
edge differs by zero.

Teleport records can be structurally correct while landing the player in
water, a wall, or a drop. Test both directions by normal activation. Walk every
modular join with normal collision and inspect environments at player eye
height; legal coordinates and lighting values do not establish playability.

## Verification ladder

Do not collapse these into one claim:

1. **Source sanity:** parse JSON, inspect IDs and dependencies, run
   `git diff --check`, and confirm the output path.
2. **Writer/readback:** regenerate from a masters-only config and verify the
   master list, record/edit counts, path, and structural validation result.
3. **Strict validation:** require all full scripts and dialogue results to
   compile with zero errors/warnings; no missing placed bases, wrong exterior
   ownership, or LAND seams; expected record counts; valid plugin/world.
4. **Semantic reinspection:** load the finished plugin and call load-order,
   quest, environment, and critical exact-record tools.
5. **Runtime smoke:** use a fresh isolated profile, reach a rendered cell, and
   audit the log for custom IDs and content-specific failures.
6. **Visual acceptance:** inspect approaches, doors, traversal, modular joins,
   water, lighting, NPC grounding/clothing, quest-object reachability, and
   teleport arrivals with normal collision.
7. **Normal-interaction golden path:** prove dialogue, activation, doors,
   movement, puzzles, pickups, combat, turn-ins, rewards, and non-repeatability.
8. **Branch isolation:** test consequential branches in fresh processes because
   journal rewinds do not reset persistent local script variables.
9. **Release audit:** record hashes and evidence, check status/diffs, and only
   then describe the work as runtime accepted.

Console-assisted setup is acceptable for isolated downstream branches only
after the normal upstream path has been proved, and it must be disclosed in the
validation notes. The branch choice or activation and its resulting state still
need to happen normally.

## Runtime harness notes

Use a task-specific temporary profile, not the normal player profile. The
source project successfully used Xvfb, SDL's X11 backend, a 1280x720 window,
Mesa llvmpipe, and `--no-sound=1` for unattended runs. The latter cannot prove
perceptual audio quality.

Capture the exact Xvfb PID and stop only that process. Avoid broad `pkill`.
Dialogue automation uses screen coordinates, so recapture after text/layout
changes. Send `xdotool type`, Return, and subsequent commands as separate
invocations because `type` can consume later tokens as literal input.

The console pauses simulation. Close it and allow frames to advance before
checking `OnDeath` or other frame-driven scripts. Collision-free movement and
console-set journals are inspection/setup aids, not evidence that the intended
route or transition works.

## Failure patterns already observed

The source project found defects that structural validation could not detect:
unsafe ferry arrivals, obscured entrances, gaps between modular cave pieces,
unreadably dark interiors, awkward item activation, puzzle pieces outside the
playable shell, badly rotated gates, unlearned dialogue topics, and death
scripts checked while the console had paused the game.

That history is the reason for the layered acceptance language:

- **manifest authored**: editable source exists;
- **plugin structurally written**: the ESM reader can reopen it;
- **compiler/world validated**: scripts and structural world checks pass;
- **visually and interactively runtime accepted**: the rendered game and normal
  interactions have actually been exercised.

Only the final phrase is end-to-end completion.

## High-value follow-up work

The strongest extensions for a large project are additional writable record
families, stable placed-reference IDs with update/delete/move operations,
reusable manifest fragments, formal integration tests for INFO ordering and
atomic recovery, quest-state graph validation, runtime assertions through Lua
or a dedicated test mod, and scene/screenshot probes for doors, water,
lighting, and NPC grounding.

For TES4/Oblivion work, first inventory this fork's actual format support and
design a separate capability boundary. Reuse the MCP transport, atomic-output
discipline, semantic inspection workflow, and verification ladder; do not reuse
TES3 record assumptions implicitly.
