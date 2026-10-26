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

## Delivery and fidelity

- Finish the current bounded chunk before reorganizing work. Choose subsequent
  chunks by the acceptance requirement or observed gameplay failure they unblock.
  Expand into a dependency only when needed for that deliverable; record adjacent
  discoveries for later.
- Combat formulas and similar numerical mechanics should behave reasonably like
  Oblivion. Bit-identical arithmetic, rounding and random-number sequences are
  not default acceptance requirements. Investigate exact details only when they
  cause a meaningful gameplay discrepancy or are explicitly requested. This
  preference supersedes older blanket exact-parity requirements in plans and
  evidence recipes; retain gameplay, state ownership and save/load correctness.
  Describe approximate behavior honestly and use justified tolerances in tests.
- Keep a concise current handoff: target gameplay course, remaining acceptance
  gaps, current chunk, reusable evidence and next action. Link historical records
  rather than duplicating them. At substantial checkpoints, check whether the
  gaps are shrinking; reassess sequencing when scope keeps growing.
- Use focused checks while developing and broad required gates at integration
  checkpoints. Reuse applicable evidence with its original provenance. Do not
  weaken unrelated acceptance requirements or call a partial course complete.
