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
component binary. Build and test the engine explicitly rather than compiling it
for every routine component change:

```powershell
./scripts/windows-dev.ps1 Build -Targets openmw,openmw-tests
./scripts/windows-dev.ps1 Test -TestBinaries openmw-tests
./scripts/windows-dev.ps1 Play
```

The engine build/test commands passed the engine checkpoint below. `Play` still
needs a rendered, normal-input runtime check.
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
Choose the authored tutorial opening or another existing inspection location:

```powershell
./scripts/windows-dev.ps1 Play -Scenario tutorial
./scripts/windows-dev.ps1 Play -Scenario creatures
./scripts/windows-dev.ps1 Play -Scenario inventory
./scripts/windows-dev.ps1 Play -Scenario city
```

Other selectors are `prison` (the default), `swimming`, `horse`, and `oblivion`.
The tutorial uses `new-game=1`; other starts are direct inspections with
`new-game=0`. The tutorial start and inspection locations follow the existing
Linux tryout presets, but do not run their automation. Full tutorial progression,
autonomous combat/death and dungeon completion remain unfinished. The creature
cave is an experimental inspection, not a prepared combat acceptance course.
Controlled M15 courses require their recipe-specific plugins and staged saves;
their Linux input/replay runners have not been ported by these selectors.

`-PrepareOnly` creates the profile without launching the game. Scenario configs
were checked on Windows without rendering; no new gameplay acceptance is claimed.
This fork's fresh Oblivion bindings use WASD/mouse to move/look, Space to
activate, Tab for inventory, E to jump, F to ready a weapon, left mouse to attack,
right mouse to block, and F5/F9 to quicksave/quickload. Esc opens the menu.
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

At this initial checkpoint the game and engine tests were not built. The engine
checkpoint below supersedes that build gap; the earlier evidence retains its
original source and selection provenance.

## Engine checkpoint — 2026-11-01

`openmw.exe` and `openmw-tests.exe` now build natively. `openmw --version`
starts, reports version 0.52.0 and exits successfully. The final combined run
passed **2,091 engine tests and 3,216 component tests**, with zero failures/skips,
exact inventory agreement and identical source fingerprints before/after.

The port retains the nonthrowing publication/retention checks. MSVC-specific
direct container swaps cover dialogue tries, Lua resources and input registries,
pathgrids, actor/object/inventory lists, the action ledger and get-up graphs.
Windows NPC and clock publication exchange fields in place. Newly cached pickup
references prepare destination storage before publication and transfer the latest
cache fields, including dropper-list updates, without allocating during commit.
That uncommon Windows pickup path uses storage proportional to the cached
reference count; Linux retains its reserved vector move. Explicit member
exchanges must be kept in sync if the owning records gain fields.

Other fixes make an OSG raw-pointer comparison and Lua test error extraction
explicit, compare hash-table preparation against the actual initial bucket count,
and keep the Windows test capture/Python verifier in UTF-8. No formulas,
serialization schemas or numeric tolerances changed. Linux's production paths
and build defaults remain intact; Linux regression/sanitizer runs were not
available on this laptop and are not claimed here.

Evidence uses a dirty tree based on `d587d4cfb3`, fingerprint
`64a06fc827665134d24e4238b1eb624d29a200dbb4cb42c49dd8b0bb59c8fc94`,
before these checkpoint notes were added. Full inventories, XML, cache, binary
hashes and source receipts are in
`build-windows/checks-8a6cb97ce13a4292b4cf02a3402505c6/`. The combined log and
structured verification summary are
`build/windows-engine-audit/combined-full-01.log` and
`build/windows-engine-audit/verification.json`. The game executable's separate
hash/startup receipt is `build/windows-engine-audit/openmw-version.json`.
Earlier failed build/test attempts remain in that audit directory.

Next action: use the existing `Play` command for an isolated prison inspection,
then check real rendering, input, inventory and save/reload. The game has not
been launched into a rendered cell in this checkpoint. Normal-interaction
gameplay, full Python, sanitizer and Linux/Morrowind runtime acceptance remain
separate gates; Linux remains the primary development environment.
