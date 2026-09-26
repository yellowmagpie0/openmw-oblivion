# Local evidence map

Repository: `/home/maciek/openmw-oblivion/openmw-oblivion`.
Original game: `/home/maciek/.local/share/Steam/steamapps/common/Oblivion`.
Original executable SHA-256:
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`.
Oblivion.esm SHA-256:
`a26e21ea8c3041f8737ffb3a266129dedb7f8a88590625ecfecd5eb7f66b4a70`.

Authoritative maintained sources (read only what the task needs):

- `docs/oblivion/M15-COMBAT-STEALTH-CRIME-IMPLEMENTATION-PLAN.md`: gates S0–S14.
- `docs/oblivion/M15-COMBAT-STEALTH-CRIME.md`: progress and evidence, not a substitute
  for checking current Git state or test results.
- `docs/oblivion/M15-NATIVE-RULE-PROVENANCE.md`: reviewed formulas/branch semantics.
- `M15-PHYSICAL-RULE-INPUTS.json`, `M15-COMBAT-STYLE-DEFAULTS.json`,
  `M15-MASTERY-ABILITY-MATRIX.json`, `M15-CASE-INVENTORY.json` in that directory.
- `M15-CAMPAIGN-COMBAT-POLICIES.md`: selected tutorial, bandit, Arena, shop/jail
  content dependencies. Unresolved narrow bound gear/abilities must be implemented
  or left open, not neutralized as generic M16 work.

Ignored evidence root: `build/oblivion-compat/m15/S2`.

- `sources-01/original-exe.json`: image metadata/section map.
- `sources-01/native-setting-initializers.json`: compiled defaults; extraction is
  incomplete, so missing entries are unknown rather than zero.
- `sources-01/xobse`, cached record-inspection sources: hints, not verified rules.
- `audit-12/m15-audit.json`: installed winning `settings` dictionary keyed by name,
  values `{key,type,value}`. A missing override uses a verified compiled default.
  Check for newer audits/content hashes; do not freeze old counts in new tests.
- `oracle-emulator/detection.py`, `pickpocket.py`: full original arithmetic and
  float/conversion setup. `fight-score.py` plus `fight-score-driver.cpp` illustrate
  comparing compiled C++ directly with independent expected values.
- `oracle-emulator/alarm-response.py`: hook-based boundary stubs with real native
  package and sleeping-state queries.
- `oracle-emulator/report-infamy.py`: multiple common report branches and actual
  infamy/conversion code. Includes a retained missing-exit-stop harness failure.
- `policy-audit-01/inspect_campaign.py`: independent raw-record walker, with all
  selected master hashes checked. Generated content is not committed.

A data audit can have zero parser/content errors and still return a nonzero
exit because runtime gates are incomplete. Read the structured result instead
of relabeling this as either full success or a parser failure. Generated local
case tables are useful for reproducibility, but retain readable source-controlled
formula/expected-boundary tests as well.
