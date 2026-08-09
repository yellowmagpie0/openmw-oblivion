# OpenMW Oblivion

OpenMW Oblivion is a clean-room compatibility fork of [OpenMW](https://www.openmw.org/)
that adds support for running the original Bethesda Oblivion (TES4) data files
while preserving OpenMW's existing Morrowind engine and save behavior.

This repository contains engine and tooling work only. It does not include
Oblivion, Morrowind, DLC, or any other proprietary game data. You must own the
game(s) and provide their data directories locally.

## Current status

The latest compatibility ledger (2026-08-08) records milestones M0 through M5
as accepted. The next bounded delivery is the M6 ObScript frontend. This is
not a complete Oblivion replacement yet; it is an incrementally verified
vertical implementation.

The accepted work currently includes:

- Separate TES3/Morrowind and TES4/Oblivion game profiles with automatic format
  detection and explicit `--game-profile` selection.
- Strict TES4 parsing, lossless preservation of deferred subrecords, and a
  complete audit of the installed official ESM/ESP and BSA files.
- Load-order-independent `FormKey` identity, typed TES4 stores, override and
  reference graphs, and deterministic restart/reorder checks.
- Versioned, content-fingerprinted TES4 runtime state in native saves,
  including player, globals, inventories, references, and mutable world state.
- The first interactive Oblivion slice: a traversable Imperial prison interior
  with collision, focus, activation, take/loot/read/harvest interactions,
  ownership and locks, animated and keyed doors, teleport traversal, and saved
  state mutations.
- A deterministic compatibility harness with JSON/HTML evidence, scenario
  manifests, visual captures, Morrowind regression tests, and ASan/UBSan
  parser coverage.

Morrowind remains a supported regression target. The project is primarily
validated against the original English Oblivion 1.2.0416 GOTY/DLC content set,
with proprietary-data fingerprints and generated evidence kept below `build/`.

## Known limitations

Oblivion support is deliberately bounded at the current milestone. NPC AI,
combat, the full Oblivion UI, native menus, complete rendering/material
parity, and ObScript execution are still future work. Placeholder UI/materials
are expected in the current prison slice.

The committed tree can also expose the following known startup compatibility
diagnostic after the prison cell loads:

```text
Failed to start new game: MagicEffect '"Shield"' not found
```

That indicates a missing shared-bootstrap compatibility record, not missing
Oblivion data. The cell may still render, but a clean interactive startup is
not yet a claim of full gameplay compatibility.

## Try the interactive slice

From the repository root, build the game binary and run the launcher:

```sh
cmake --build build --target openmw -j2
./scripts/run-oblivion.sh "/path/to/Oblivion/Data"
```

With the standard Steam installation, the data argument can be omitted:

```sh
./scripts/run-oblivion.sh
```

The launcher creates an isolated configuration, starts the Oblivion profile in
the prison interior, and keeps saves/screenshots under
`build/oblivion-userdata`. You can instead set `OBLIVION_DATA`,
`OPENMW_BIN`, `OPENMW_RESOURCES`, or `OPENMW_OBLIVION_USER_DATA`.

Use the mouse to look, `W/A/S/D` to move, `Space` to activate, `F5` to
quicksave, and `Esc` to quit.

The launcher is a development entry point, not a packaged end-user build. For
the deterministic visual smoke test and the complete M5 acceptance campaign,
see [`docs/oblivion/M5-INTERACTIVE-INTERIOR.md`](docs/oblivion/M5-INTERACTIVE-INTERIOR.md).

## Build and test

The normal OpenMW CMake workflow remains the foundation. A typical focused
build is:

```sh
cmake -S . -B build \
  -DBUILD_COMPONENTS_TESTS=ON \
  -DBUILD_OPENMW_TESTS=ON
cmake --build build --target openmw components-tests openmw-tests esmtool bsatool -j2
```

Run the focused Oblivion profile test with:

```sh
./build/openmw-tests --gtest_filter='OblivionProfileServicesTest.*'
```

Run a full installed-data census with:

```sh
python3 scripts/oblivion_compat.py baseline \
  --build build \
  --oblivion-data "/path/to/Oblivion/Data" \
  --census-all --require-lossless-tes4 \
  --output build/oblivion-compat/baseline
```

The machine-readable status ledger, milestone reports, exact acceptance
commands, and generated-evidence conventions are documented in
[`docs/oblivion/README.md`](docs/oblivion/README.md) and
[`docs/OBLIVION-COMPATIBILITY-ROADMAP.md`](docs/OBLIVION-COMPATIBILITY-ROADMAP.md).

## Architecture in brief

TES3 and TES4 readers remain separate. Native Oblivion records are not
pretended to be Morrowind records; format-specific data is adapted only at
reviewed runtime boundaries. Stable `FormKey` identities avoid persisting
load-order indices, and TES4 runtime state uses content fingerprints so saves
fail clearly when their required content is absent or changed.

The scenario harness treats proprietary data as an external input and writes
reports, logs, screenshots, and temporary runtime state below `build/`.
Unknown official subrecords are preserved or explicitly allowlisted rather
than silently discarded.

## Upstream, license, and attribution

This project is based on OpenMW. OpenMW and this fork are licensed under the
GPLv3; see [`LICENSE`](LICENSE). OpenMW's upstream project, documentation, and
community resources are available at <https://www.openmw.org/> and
<https://openmw.readthedocs.io/>.

Font licenses shipped with the inherited OpenMW resources remain in:

- `files/data/fonts/DejaVuFontLicense.txt`
- `files/data/fonts/DemonicLettersFontLicense.txt`
- `files/data/fonts/MysticCardsFontLicense.txt`
