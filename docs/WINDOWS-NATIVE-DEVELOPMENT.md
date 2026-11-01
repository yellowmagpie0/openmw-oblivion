# Native Windows development

This is an optional MSVC setup. Linux configuration, CI and acceptance courses
remain unchanged. Use a separate branch for Windows portability fixes. Native
outputs live in ignored `build-windows/`; dependency packages live in ignored
`deps/`. Never copy a Linux CMake cache into this build or share generated build
directories between machines.

## Toolchain

In PowerShell, install the VS 2022 C++ workload (including the Windows SDK),
CMake, Ninja, 7-Zip and Python 3.12. The installation may request elevation.

```powershell
winget install --id Microsoft.VisualStudio.2022.BuildTools --exact --source winget --override '--quiet --wait --norestart --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended'
winget install --id Kitware.CMake --exact --source winget
winget install --id Ninja-build.Ninja --exact --source winget
winget install --id 7zip.7zip --exact --source winget
winget install --id Python.Python.3.12 --exact --source winget
```

The helper supports Windows PowerShell 5.1 and PowerShell 7. It discovers VS 2022 and activates
its x64 compiler environment for the current process. A pre-existing Developer
PowerShell session is unnecessary. It also uses VS's bundled Ninja and can fall
back to VS's bundled CMake. Python is located in its default per-user installation.
If script execution is disabled in a terminal, invoke it with a process-local
policy instead of changing machine policy:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ./scripts/windows-dev.ps1 Build
```

## Dependency bundle

The versions and archive provenance come from `CI/deps_versions.msvc.sh` and
`.github/workflows/windows.yml`: MSVC 2022, vcpkg bundle `2026-02-24`.
From the checkout root:

```powershell
New-Item -ItemType Directory -Force deps/windows-downloads | Out-Null
$manifest = 'deps/windows-downloads/vcpkg-manifest.txt'
Invoke-WebRequest 'https://gitlab.com/OpenMW/openmw-deps/-/raw/main/windows/vcpkg-x64-windows-2022-2026-02-24-manifest.txt' -OutFile $manifest
$lines = Get-Content $manifest
$parts = $lines[1] -split '\s+'
$archive = Join-Path 'deps/windows-downloads' $parts[1]
Invoke-WebRequest $lines[0] -OutFile $archive
if ((Get-FileHash $archive -Algorithm SHA512).Hash -ne $parts[0]) { throw 'Archive hash mismatch' }
& 'C:/Program Files/7-Zip/7z.exe' x -y '-odeps/vcpkg-x64-windows-2022-2026-02-24' $archive
if ($LASTEXITCODE -ne 0) { throw 'Extraction failed' }
```

The first configure also downloads hash-pinned sources. It builds double-precision
Bullet and the fork's patched static MyGUI rather than using the bundle's MyGUI
binary. Bullet's MSVC default disables RTTI; this setup keeps RTTI enabled because
the fork dynamically identifies native physics worlds and dispatchers.
CMake 4 receives the compatibility policy floor needed by older bundled
dependencies. Qt tools/editor are disabled for this initial engine/test setup.

## Daily commands

```powershell
./scripts/windows-dev.ps1 Configure
./scripts/windows-dev.ps1 Build
./scripts/windows-dev.ps1 Build -Targets components-tests -Jobs 2
./scripts/windows-dev.ps1 Test -TestBinaries components-tests -Filter 'ESM4*'
./scripts/windows-dev.ps1 Test
./scripts/windows-dev.ps1 Python
```

`Build` defaults to `components-tests` and `esmtool`; `Test` defaults to the
component binary. Engine and engine-test targets are configured for an explicit
next checkpoint, rather than compiled by every routine component build:

```powershell
./scripts/windows-dev.ps1 Build -Targets openmw,openmw-tests
./scripts/windows-dev.ps1 Test -TestBinaries openmw-tests
./scripts/windows-dev.ps1 Play
```

The engine commands are not established as working by component-only checks.
Two compilation jobs are the conservative default for the 16 GB laptop.
The helper adds runtime dependency directories to the current process's PATH;
executables launched directly from Explorer may need DLL deployment instead.
`Test` builds the selected binaries first, saves the unfiltered inventories and selected XML results to a fresh
directory, and verifies nonempty selection and exact inventory/result agreement.
It also records source fingerprints before/after testing, binary SHA-256 hashes
and a copy of the CMake cache. Do not edit source or commit during a test run.
`Python` deliberately runs only the portable save-state and crime-contract suites.
It is not the full Python gate.

`Play` starts a fresh prison inspection profile, loading only `Oblivion.esm`.
Pass `-OblivionData 'D:/Games/Oblivion/Data'` for another installation. Its config
and user-data directory are unique per launch and retained under `build-windows`.
It uses the current build in place: finish building before playing. This is a
manual inspection start, not proof of normal tutorial progression or combat.

## Scope and evidence

The full Python suite and automated runtime courses still contain Linux-specific
process, X11, compiler and input tooling. Windows checks do not replace Linux
ASan/UBSan, Morrowind regression, original-game oracle or normal-input gameplay
acceptance. Keep proprietary game assets, saves and generated evidence local.
Record the tested source revision, selection and binary hashes when using native
results as implementation evidence. Do not infer runtime acceptance from a build.

The MSVC port keeps the Lua runtime variant behind an owning pointer so retention
can exchange loaded/unloaded owners without throwing. Non-MSVC builds retain
their original inline storage. The static safety assertion remains enabled, and
the component suite exercises loaded/unloaded exchange and existing retention
behavior. This does not establish an engine-wide Windows runtime port.

## Laptop checkpoint — 2026-11-01

Installed VS Build Tools 2022 17.14.44 (MSVC 19.44), the Windows SDK, CMake 4.4.4,
Ninja 1.13.2, 7-Zip 26.05 and Python 3.12.10. The dependency archive was checked
against the CI manifest's SHA-512 before extraction. Configure and the native
helper were exercised through Windows PowerShell 5.1 and PowerShell 7.

- `components-tests` and `esmtool` compile/link natively with two jobs.
- All **3,213 native component tests** passed, with exact inventory agreement,
  no failures/skips and identical before/after source fingerprints.
- The unchanged portable Python selection passed **161 tests** under the
  installed Python runtime. This is separate from the full Python gate.
- Native `esmtool dump -q` read the installed `Oblivion.esm` successfully.
- The broader component run uncovered an uninitialized fourth lane in one
  Bullet test vector. Initializing that fixture preserves every exact assertion;
  no gameplay formula or tolerance was changed.

The full C++ run used a dirty tree based on `6200b7fce1`, fingerprint
`76f9421aa99f0d3847c6d0bbb85197d3ea200b6426efdcda3474381b713b5b45`.
Its inventory, XML, cache, binary hash and source receipts are retained in
`build-windows/checks-30455e8cdec54fadac41505aac5245ed/`; console output is in
`build/windows-audit/full-cpp-tests-03.log`. These identify the tested tree before
this checkpoint text was added. Earlier failed attempts remain in
`build/windows-audit/`. The Python log is
`build/windows-audit/python-standalone-02.log`.

Next checkpoint: build `openmw`/`openmw-tests` and address any additional MSVC
portability gaps before attempting an isolated Windows runtime course. No game
binary, engine-test, sanitizer, full Python, Linux regression or gameplay
acceptance is claimed by this checkpoint. Linux remains the primary environment.
