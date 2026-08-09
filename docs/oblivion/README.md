# Oblivion compatibility evidence

This directory contains durable milestone reports for
[`OBLIVION-COMPATIBILITY-ROADMAP.md`](../OBLIVION-COMPATIBILITY-ROADMAP.md).
Generated JSON, HTML, logs, screenshots, videos, original game assets, and
original-game captures stay below the build directory and are not committed.

Run the M0 baseline audit with:

```sh
python3 scripts/oblivion_compat.py baseline \
  --build build \
  --oblivion-data "/path/to/Oblivion/Data" \
  --morrowind-data "/path/to/Morrowind/Data Files" \
  --run-standalone \
  --output build/oblivion-compat/m0
```

Use `--hash-archives` for release evidence. The default hashes every ESM/ESP
but records only size and modification time for large BSA archives.

Run the accepted M2 stable identity/reference graph gate with:

```sh
python3 scripts/oblivion_compat.py form-graph \
  --esmtool build/esmtool \
  --oblivion-data "/path/to/Oblivion/Data" \
  --output build/oblivion-compat/m2-acceptance
```

This scans the canonical eleven-plugin official set, compares the complete
graph after binary restart and runtime-index reordering, validates every
unresolved edge against the count-locked reviewed exception file, and writes
`acceptance.json` plus a directly inspectable `acceptance.html`.

Run the accepted M3 standalone boot gate with:

```sh
python3 scripts/oblivion_compat.py m3-acceptance \
  --build build \
  --oblivion-data "/path/to/Oblivion/Data" \
  --morrowind-data "/path/to/Morrowind/Data Files" \
  --output build/oblivion-compat/m3-acceptance
```

This performs the Oblivion interior/exterior visual boots, explicit profile
failure, Morrowind visual save/load, focused service tests, content
fingerprints, and the complete Morrowind integration suite.

Run the accepted M5 interactive-prison gate with the full original-game visual
oracle using the command in
[`M5-INTERACTIVE-INTERIOR.md`](M5-INTERACTIVE-INTERIOR.md). It covers normal
input, collision, take/loot/read/harvest, locks, ownership, animated and
teleport doors, native save state, fixed original/OpenMW captures, the stable
official FormKey graph, and Morrowind regressions.

To try the first interactive slice directly:

```sh
cmake --build build --target openmw -j2
./scripts/run-oblivion.sh "/path/to/Oblivion/Data"
```

Use the mouse to look, `W/A/S/D` to move, `Space` to activate, `F5` to
quicksave, and `Esc` to quit. User data stays below
`build/oblivion-userdata` by default.

Scenario manifests use
[`scenario.schema.json`](../../scripts/data/oblivion_compat/scenario.schema.json).
For example, the hermetic runner check is:

```sh
python3 scripts/oblivion_compat.py scenario \
  scripts/data/oblivion_compat/self_test_scenario.json \
  --output build/oblivion-compat/self-test \
  --variable "python=$(command -v python3)"
```

Scenarios can also use `assert_file` with an output-relative glob, size bound,
expected count, and required/forbidden ASCII markers. M4 uses this to verify
that a normal-input quicksave contains its profile and native TES4 state
envelope. File assertions may not escape the scenario output.

Every milestone report must name the exact source revision and content
fingerprints, list commands and pass/fail counts, link generated evidence by
its build-relative location, include direct visual-inspection notes, and
separate current limitations from regressions.

Current implementation reports:

- [`IMPLEMENTATION-STATUS.json`](IMPLEMENTATION-STATUS.json) is the
  machine-readable resume ledger.
- [`M0-BASELINE.md`](M0-BASELINE.md)
- [`M1-TES4-PARSING.md`](M1-TES4-PARSING.md)
- [`M2-FORM-IDENTITY.md`](M2-FORM-IDENTITY.md)
- [`M3-STANDALONE-BOOT.md`](M3-STANDALONE-BOOT.md)
- [`M4-RUNTIME-STATE.md`](M4-RUNTIME-STATE.md)
- [`M5-INTERACTIVE-INTERIOR.md`](M5-INTERACTIVE-INTERIOR.md)

The roadmap ledger distinguishes accepted milestones from foundations that
have begun but have not passed their complete content/runtime acceptance gate.
M2 is accepted by its full official-content graph and runtime regressions, not
merely by a focused unit suite.

The checked-in `oblivion_standalone_baseline.json` manifest creates its config
inside the output directory, launches only `Oblivion.esm` under Xvfb, relies
on profile-owned archive defaults, records engine statistics, and captures a
deterministic 1280x720 baseline. The stricter M3 interior and exterior
manifests additionally assert native-service logs and reject runtime errors.

`oblivion_original_baseline.json` runs the installed original executable under
an isolated Proton compatibility prefix below the scenario output directory.
It never writes into the source tree or treats cross-engine pixel identity as
an acceptance criterion.
