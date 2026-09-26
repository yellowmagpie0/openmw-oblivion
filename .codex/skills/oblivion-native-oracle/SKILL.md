---
name: oblivion-native-oracle
description: Independently verify Oblivion TES4 mechanics using hash-identified original executable instructions, winning-record audits, and isolated original-game probes. Use when implementing native rules or resolving rounding, branch, actor-identity or gameplay discrepancies in this fork.
---

Use this skill to establish expected behavior independently of production C++.
Do not call the implementation to generate its own expected values, infer a
rule from a misleading reverse-engineering label, or mark a runtime gate passed
from emulated arithmetic alone.

## Select the evidence needed

- **Rules/branches:** read [references/instruction-oracles.md](references/instruction-oracles.md).
  Use the original executable's full relevant instruction path where practical;
  explicitly identify any boundary stubs. Verify registers, caller arguments,
  virtual dispatch, storage and return conventions before assigning semantics.
- **Real normal-input observations:** read [references/live-probes.md](references/live-probes.md).
  Keep isolated prefix/config/save/plugin inputs and declare tolerances before
  running. Preserve failures and pristine saves.
- **Content/defaults:** read [references/evidence-map.md](references/evidence-map.md).
  Distinguish compiled defaults, winning installed records, live GMST reads,
  independent formula expectations and actual gameplay acceptance.

The local game path is a convenience, never an identity check. Use
`scripts/inspect_pe.py --exe PATH --sha256 EXPECTED --output NEW-METADATA` to
verify a PE32/i386 image and record section mappings without copying executable
bytes. The known original 1.2.0416 SHA-256 is in the evidence reference. Never
reuse its addresses against another binary merely because its version string
matches. On mismatch, stop the address-based experiment and identify the image.

Implement reviewed formulas as immutable input/output rules in `components/esm4`.
Use double intermediates and explicit float stores only where the native path
requires them; inspect integer conversion and branch flags. Test deterministic
boundaries first, then generated comparisons/probability distributions as
required by the plan. Validate malformed supported-domain inputs explicitly.
Keep unknown semantics open rather than tuning constants to one observation.

Publish concise provenance, expected values and editable probe manifests in
Git. Proprietary game files, extracted scripts/disassembly, images, generated
plugins and bulky generated oracle corpora stay outside Git in ignored evidence
directories. Record executable/content hashes, case counts, boundary stubs and
retained failures. After the rule passes, integrate it into the actual runtime
authority and complete the plan's normal-input and fresh-process acceptance
cases; pure helpers are an intermediate result.
