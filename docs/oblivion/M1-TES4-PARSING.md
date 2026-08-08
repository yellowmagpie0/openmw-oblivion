# M1 strict TES4 parsing and record census

Status: implementation and functional acceptance complete on 2026-08-08.
ASan/UBSan execution is an environmental exception, not a silent pass.

## Delivered source

- `components/esm4/reader.*` now validates raw subrecord boundaries,
  `XXXX` extended sizes, compressed wrapper sizes, exact inflate output and
  stream termination, and truncated input.
- `components/esm4/loadrawrecord.*` losslessly retains every logical
  subrecord payload for official record families that do not yet have a typed
  semantic loader. Record flags, IDs, stable form keys, editor IDs, names, and
  extended payloads remain inspectable.
- `apps/esmtool/tes4.cpp` routes the 13 formerly unsupported official TES4
  families through the raw record path.
- Reader skip instrumentation counts skipped record/subrecord pairs and bytes.
  `scripts/data/oblivion_compat/tes4_skipped_subrecords.json` is the reviewed
  88-pair allowlist; the baseline fails on an unlisted pair.
- `apps/components_tests/esm4/rawrecord.cpp` contains ordinary, compressed,
  extended-size, truncation, malformed-boundary, trailing-stream, and bounded
  mutation-corpus cases.

## Verification and evidence

```sh
cmake --build build --target esmtool components-tests -j2
./build/components-tests --gtest_filter='ESM4RawRecord.*'
python3 scripts/oblivion_compat.py baseline \
  --build build \
  --oblivion-data "/home/maciek/.local/share/Steam/steamapps/common/Oblivion/Data" \
  --morrowind-data "/home/maciek/.local/share/Steam/steamapps/common/Morrowind/Data Files" \
  --census-all --require-lossless-tes4 \
  --output build/oblivion-compat/m1
```

Recorded again after the final strict-boundary changes in
`build/oblivion-compat/m1-acceptance/baseline.json` and `.html`:

- 11/11 installed official ESM/ESP files parsed and censused.
- 17/17 installed BSA archives listed.
- No unsupported record family, census failure, unallowlisted subrecord skip,
  assertion, or parser crash.
- 88 observed skipped subrecord pairs, exactly matching the reviewed
  allowlist.
- 14/14 Morrowind integration tests passed.
- The complete component suite passed at the M1 checkpoint, in addition to the
  focused malformed-input corpus.

## Sanitizer exception

The sanitizer build was attempted. This Fedora installation exposes GCC
linker-script references to absent `libasan.so.8.0.0` and
`libubsan.so.1.0.0`; compiler-rt libraries exist but no usable Clang compiler
binary is installed. Consequently ASan/UBSan could not link. This remains an
acceptance-infrastructure item to rerun when the host toolchain is repaired.
It does not weaken the normal-build mutation and official-content gates.

## Success assessment

M1 passes the defined functional gate: all locally installed official content
is either typed or losslessly represented, all typed omissions are observable
and allowlisted, repeated counts are deterministic, malformed synthetic input
is rejected, and the Morrowind gate is green. Raw preservation is not a claim
that those 13 families already have gameplay semantics; their typed rollout is
tracked by later milestones.
