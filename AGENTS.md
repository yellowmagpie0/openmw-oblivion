# Local workspace guidance

Before changing content-authoring support or the local OpenMW-CS MCP server,
read `docs/DEVELOPMENT-HANDBOOK.md`. It is the local handoff for provenance,
build configuration, MCP usage, authoring conventions, validation layers, and
known limitations.

- Preserve unrelated user changes and do not push or publish local work unless
  explicitly requested.
- Keep declarative manifests as editable source. Treat generated plugins as
  reproducible artifacts, not as the only source of a change.
- Distinguish structural write success, compiler/world validation, semantic
  reinspection, and actual runtime acceptance in completion reports.
- The imported MCP implementation currently targets TES3/Morrowind records.
  Do not imply that it supports Oblivion/TES4 data until that support is
  implemented and verified in this fork.
