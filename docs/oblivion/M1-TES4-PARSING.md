# M1 strict TES4 parsing and record census

Status: implementation, functional acceptance, and sanitizer acceptance complete
on 2026-08-08.

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

## Sanitizer verification

The Fedora host toolchain was repaired by installing the GCC 16.1.1
`libasan` and `libubsan` runtime packages. The combined ASan/UBSan build and M1
checks were then run with immediate failure and leak detection enabled:

```sh
cmake -S . -B build-oblivion-sanitized \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_COMPONENTS_TESTS=ON \
  -DOPENMW_USE_SYSTEM_BULLET=OFF \
  -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' \
  -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined' \
  -DCMAKE_SHARED_LINKER_FLAGS='-fsanitize=address,undefined'
cmake --build build-oblivion-sanitized \
  --target components-tests esmtool bsatool -j2
ASAN_OPTIONS='detect_leaks=1:halt_on_error=1:abort_on_error=1' \
UBSAN_OPTIONS='print_stacktrace=1:halt_on_error=1' \
  ./build-oblivion-sanitized/components-tests \
  --gtest_filter='ESM4RawRecord.*'
ASAN_OPTIONS='detect_leaks=1:halt_on_error=1:abort_on_error=1' \
UBSAN_OPTIONS='print_stacktrace=1:halt_on_error=1' \
  python3 scripts/oblivion_compat.py baseline \
  --build build-oblivion-sanitized \
  --oblivion-data "/home/maciek/.local/share/Steam/steamapps/common/Oblivion/Data" \
  --census-all --require-lossless-tes4 \
  --output build/oblivion-compat/m1-sanitized
```

All 10 focused raw-record, malformed-input, compressed-stream, and bounded
mutation-corpus tests passed without an ASan, UBSan, or leak finding. The
sanitized official-content run also passed: 11/11 plugins parsed and censused,
17/17 archives listed, all 88 observed skipped-subrecord pairs matched the
allowlist, and no unsupported record was found. The durable local report is
`build/oblivion-compat/m1-sanitized/baseline.json` and `.html`; focused test
results are in `build/oblivion-compat/m1-sanitized/components-tests.xml`.

## Success assessment

M1 passes the defined functional gate: all locally installed official content
is either typed or losslessly represented, all typed omissions are observable
and allowlisted, repeated counts are deterministic, malformed synthetic input
is rejected, and the Morrowind gate is green. Raw preservation is not a claim
that those 13 families already have gameplay semantics; their typed rollout is
tracked by later milestones.
