# OpenMW-CS MCP server

`openmw-cs-mcp` is a headless Model Context Protocol server for inspecting and
patching Morrowind quests with the ESM3 reader and writer used by OpenMW and
OpenMW-CS. It supports MCP over stdio and reads the effective `data=` and
`content=` load order from `openmw.cfg`.

The write tool never edits a loaded ESM or ESP. It creates a separate ESP patch
with the loaded content files as masters and validates the result by reopening
it with OpenMW's ESM reader.

## Build and run

Build OpenMW with OpenMW-CS enabled, then build the server target:

```sh
cmake -S . -B build -DBUILD_OPENCS=ON
cmake --build build --target openmw-cs-mcp
```

The server uses the normal user config by default:

```sh
./build/openmw-cs-mcp
```

An MCP client can register it as a stdio command. The generic configuration is:

```json
{
  "command": "/absolute/path/to/openmw/build/openmw-cs-mcp",
  "args": ["--config", "/absolute/path/to/openmw.cfg"]
}
```

Only MCP JSON-RPC messages are written to stdout. Load diagnostics are written
to stderr.

## Tools

- `get_load_order`: report the data files and resolved paths.
- `find_quests`: search journal IDs, quest names, and journal text.
- `get_quest`: return journal stages and dialogue/scripts that directly refer
  to the quest ID.
- `get_area_quests`: discover quests connected to actors and scripted objects
  placed in cells matching a town or area name.
- `find_environments`: find exterior and interior cells by name, region, or ID
  and summarize their placements.
- `get_environment`: inspect structured CELL references, terrain height and
  texture samples, placement bounds, terrain-relative object heights, doors,
  teleport destinations, and local NPC definitions.
- `search_records`: search dialogue, scripts, and textual ESM subrecords. This
  is useful for following item, NPC, container, and cell IDs discovered in a
  quest.
- `get_record`: return complete structured dialogue/script data or indexed text
  fields for another record type.
- `write_quest_patch`: create a separate ESP containing atomic quest edits.
- `write_world_plugin`: create a separate ESP containing terrain, cells,
  placed references, NPCs, items, scripts, dialogue, and quests.
- `validate_plugin`: reopen a generated plugin, report its masters and
  structured quest/dialogue/script records, and compile all dialogue results
  and full scripts with OpenMW's compiler. It also validates placed base IDs,
  exterior-cell ownership, and shared LAND edges. Diagnostics include source
  record, line, column, severity, and offending literal.

`write_quest_patch` accepts these edit operations:

- `add_dialogue`: create a new topic, greeting, or quest journal.
- `update_info`: change an existing journal or dialogue INFO.
- `add_info`: insert a new journal/dialogue INFO and update its linked-list
  neighbours.
- `delete_info`: write a deleted INFO override.
- `update_script`: replace an existing script's source text. The compiled
  bytecode is cleared so OpenMW compiles the source. Use `validate_plugin`
  after writing to perform semantic compilation.
- `add_script`: add a new source-only script record.
- `add_npc`: clone a known-good NPC template and selectively replace identity,
  stats, inventory, AI, and attached script.
- `add_misc`: clone a miscellaneous item for unique quest objects.
- `add_land` / `update_land`: write an explicit 65x65 height map or generate a
  seamless flat/island terrain surface in world coordinates.
- `add_cell` / `update_cell`: create or extend exterior/interior cells with
  positioned, scaled, owned, locked, or teleporting references.

Example tool arguments that add a journal stage:

```json
{
  "output_path": "/mods/MyQuestPatch.esp",
  "author": "Example",
  "edits": [
    {
      "op": "add_info",
      "topic_id": "MS_FargothRing",
      "info_id": "my_mod_fargoth_stage_50",
      "journal_index": 50,
      "response": "I am carrying Fargoth's engraved ring."
    }
  ]
}
```

For dialogue conditions, use objects with `function`, `variable`, `comparison`,
and `value`. Named functions include `global`, `local`, `journal`, `item`,
`dead`, and the `not_*` filters. Engine function indices can be supplied as an
integer or a string such as `function_50`.

## Current scope

The server reads structured TES3 `DIAL`, `INFO`, `SCPT`, `NPC_`, `MISC`, `CELL`,
and `LAND` data. It writes those same record families to separate ESPs and uses
existing master assets for placed statics, doors, activators, containers,
lights, flora, and other references. Base content files are never modified.
