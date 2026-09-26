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

Reusable workflows for this fork are versioned under `.codex/skills/`:

- `openmw-build-test`: builds, sanitizer checks, exact test inventories and evidence recording.
- `oblivion-native-oracle`: independent original-executable rule checks and isolated game probes.

Read the relevant `SKILL.md` when using these workflows. The personal skill
installation may link here; pass the actual checkout path to helper scripts.
