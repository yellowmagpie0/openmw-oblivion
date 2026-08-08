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

The roadmap ledger distinguishes accepted milestones from foundations that
have begun but have not passed their complete content/runtime acceptance gate.
Do not infer milestone completion merely from a green focused unit suite.

The checked-in `oblivion_standalone_baseline.json` manifest creates its config
inside the output directory, launches only `Oblivion.esm` and the base-game
archives under Xvfb, records engine statistics, and captures a deterministic
1280x720 baseline. It intentionally documents the current pre-M3 behavior; it
does not assert that standalone gameplay already works.

`oblivion_original_baseline.json` runs the installed original executable under
an isolated Proton compatibility prefix below the scenario output directory.
It never writes into the source tree or treats cross-engine pixel identity as
an acceptance criterion.
