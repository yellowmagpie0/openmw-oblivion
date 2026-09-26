# Local configurations and diagnosis

These are proven local configurations, not immutable requirements. Inspect
`CMakeCache.txt`, compiler availability and repository instructions before
reconfiguring; preserve unrelated user options.

- Main `build`: GCC, RelWithDebInfo, C++20, `BUILD_COMPONENTS_TESTS=ON`,
  `BUILD_OPENMW_TESTS=ON`, `BUILD_OPENMW=ON`. Binary targets are at the build root.
- `build/m15-sanitize`: Debug with `CMAKE_CXX_FLAGS_DEBUG=-O1 -g`,
  `CMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer`,
  `CMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined`,
  `BUILD_COMPONENTS_TESTS=ON`, `BUILD_ESMTOOL=ON`, engine/editor/tests off.
- Both use `OPENMW_USE_SYSTEM_BULLET=OFF`, the fetched double-precision Bullet
  dependency. System single-precision Bullet is not interchangeable. Check
  actual options and link inputs, not merely the presence of system libraries
  in cache entries.
- The sanitizer configuration reuses `build/extern/fetched/bullet` through
  `FETCHCONTENT_SOURCE_DIR_BULLET`. Do not assume the fetched sources exist in a
  fresh checkout. Follow the repo dependency/bootstrap configuration first.
- Six build jobs have worked on this machine (about 31 GiB RAM). Adapt to actual
  resources, especially when compiling main and sanitizer builds concurrently.

Useful queries:

```bash
rg 'CMAKE_(BUILD_TYPE|CXX_COMPILER|CXX_FLAGS|HOME_DIRECTORY)|BUILD_(COMPONENTS_TESTS|OPENMW_TESTS)|OPENMW_USE_SYSTEM_BULLET|FETCHCONTENT_SOURCE_DIR' build/CMakeCache.txt
cmake --build build --target components-tests -j6
cmake --build build --target openmw openmw-tests esmtool -j6
```

Register a new ESM4 implementation in `components/CMakeLists.txt` and its test
in `apps/components_tests/CMakeLists.txt`; the test filenames use names such as
`esm4/crimerules.cpp`, not `testcrimerules.cpp`. Native settings adapters live in
`components/esm4/combatsettings.*` and consume typed winning TES4 GMST records.

Logs may contain existing sol3/animation warnings. Preserve and distinguish
those from new warnings rather than claiming warning-free builds. A green
incremental GCC build does not satisfy the plan's fresh GCC/Clang, editor,
external-Lua, Morrowind regression, media/performance or soak gates.

If a formerly valid build fails at link with a missing versioned system library
(e.g. an old `/usr/lib64/libSDL2-2.0.so.*` after an OS package update), preserve
the failure log, inspect installed libraries, and rerun `cmake -S . -B BUILD_DIR`
without changing unrelated cached options. This refreshes imported library
paths. Do not invent a compatibility symlink to a different ABI or report old
test results as a new pass. Recheck the configuration and use fresh evidence
for the rebuilt tests.
