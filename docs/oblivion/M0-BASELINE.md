# M0 reproducible baseline and acceptance harness

Status: implementation complete; acceptance evidence recorded on 2026-08-08.

## Delivered source

- `scripts/oblivion_compat.py` provides content fingerprinting, typed ESM scan
  orchestration, BSA inventory, deterministic scenario execution, isolated
  Xvfb lifecycle management, structured log checks, image sanity checks,
  SSIM/dHash/pixel-change comparison, Morrowind regression execution, and
  atomic JSON plus HTML reporting.
- Scenario schema and hermetic self-test, standalone Oblivion baseline, and
  original-Proton baseline manifests live in `scripts/data/oblivion_compat`.
- `scripts/tests/test_oblivion_compat.py` characterizes the harness, including
  intentional failure detection.

Generated evidence is in `build/oblivion-compat/m0` and is intentionally
ignored by Git. The authoritative structured report is `baseline.json`; the
human-readable rendering is `baseline.html`.

## Verification

Commands:

```sh
python3 -m unittest scripts.tests.test_oblivion_compat -v

python3 scripts/oblivion_compat.py baseline \
  --build build \
  --oblivion-data "/home/maciek/.local/share/Steam/steamapps/common/Oblivion/Data" \
  --morrowind-data "/home/maciek/.local/share/Steam/steamapps/common/Morrowind/Data Files" \
  --run-standalone \
  --output build/oblivion-compat/m0
```

Observed results:

- Harness unit tests: 9 passed, 0 failed.
- Oblivion content: 11/11 ESM/ESP files parsed; 17/17 BSA files listed.
- Morrowind integration: 14/14 tests passed.
- Main-master typed census: 1,162,732 parsed records and 4,283 skipped records
  in 13 explicit record families. This is the input to M1.
- The standalone process loaded `Oblivion.esm` without crashing, but its
  captured 1280x720 frame was uniformly black. Automated inspection rejected
  it with mean 0 and entropy 0.
- The original-game Proton driver created an isolated prefix and captured a
  visible frame. Its first direct-executable probe reached Steam's application
  load error rather than the game; the driver records this oracle-environment
  prerequisite separately from OpenMW compatibility.

## Direct visual inspection

The standalone capture was opened at original resolution. It is a fully black
frame with no visible UI, scene, cursor, or loading image. This is an expected
pre-M3 capability failure, not visual acceptance. The important M0 result is
that the harness found and recorded the failure automatically rather than
mistaking process survival for a rendered game.

## Known baseline gaps

- Typed parsing skips `BSGN`, `CLMT`, `CSTY`, `EFSH`, `ENCH`, `FACT`, `LSCR`,
  `LVSP`, `MGEF`, `SKIL`, `SPEL`, `WATR`, and `WTHR`.
- Standalone startup still loads the Morrowind VFS resource layer and emits
  missing Morrowind UI texture diagnostics.
- Standalone startup exits before producing a playable frame.
- Original-game captures are local evidence only and are never committed.

M0 is successful because the required one-command evidence pipeline is
reproducible, its positive and negative checks are exercised, the current
failures are machine-readable, and the Morrowind regression gate is green.
