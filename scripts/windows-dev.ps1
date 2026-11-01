[CmdletBinding()]
param(
    [ValidateSet('Configure', 'Build', 'Test', 'Python', 'Play')]
    [string]$Action = 'Build',
    [string[]]$Targets = @('components-tests', 'esmtool'),
    [ValidateRange(1, 64)][int]$Jobs = 2,
    [string[]]$TestBinaries = @('components-tests'),
    [string]$Filter = '*',
    [string]$OblivionData = 'C:/Program Files (x86)/Steam/steamapps/common/Oblivion/Data'
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$build = Join-Path $repo 'build-windows'
$deps = Join-Path $repo 'deps/vcpkg-x64-windows-2022-2026-02-24'
$prefix = Join-Path $deps 'installed/x64-windows'
New-Item -ItemType Directory -Force "$build/tmp" | Out-Null
# Keep temporary writes inside the checkout, including in sandboxed sessions.
$env:TEMP = "$build/tmp"
$env:TMP = $env:TEMP

function Invoke-Checked {
    param([string]$Program, [string[]]$Arguments)
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program exited with $LASTEXITCODE" }
}

function Write-Utf8 {
    param([string]$Path, [Parameter(ValueFromPipeline)][string]$InputObject)
    begin { $lines = [System.Collections.Generic.List[string]]::new() }
    process { $lines.Add($InputObject) }
    end { [System.IO.File]::WriteAllLines($Path, $lines, [System.Text.UTF8Encoding]::new($false)) }
}

$python = Join-Path $env:USERPROFILE 'AppData/Local/Programs/Python/Python312/python.exe'
if ($Action -eq 'Python') {
    if (!(Test-Path $python)) { throw "Install Python 3.12: $python is missing" }
    Push-Location $repo
    try {
        Invoke-Checked $python @('-B', '-m', 'unittest',
            'scripts.tests.test_tes4_runtime_state', 'scripts.tests.test_tes4_crime_contracts')
    } finally { Pop-Location }
    return
}

$vswhere = "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
if (!(Test-Path $vswhere)) { throw 'Install Visual Studio 2022 Build Tools with the C++ workload.' }
$vs = & $vswhere -latest -version '[17.0,18.0)' -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vs) { throw 'Visual Studio 2022 C++ tools were not found.' }
& "$vs/Common7/Tools/Launch-VsDevShell.ps1" -Arch amd64 -HostArch amd64 -SkipAutomaticLocation
$cmake = "$env:ProgramFiles/CMake/bin/cmake.exe"
if (!(Test-Path $cmake)) { $cmake = "$vs/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe" }
$env:PATH = "$vs/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja;$prefix/bin;$prefix/bin/Release;$env:PATH"
$env:OSG_LIBRARY_PATH = "$prefix/bin/osgPlugins-3.6.5"
if (!(Test-Path "$deps/scripts/buildsystems/vcpkg.cmake")) {
    throw "Extract the pinned dependency bundle into $deps; see docs/WINDOWS-NATIVE-DEVELOPMENT.md"
}

switch ($Action) {
    Configure {
        Invoke-Checked $cmake @('-S', $repo, '-B', $build, '-G', 'Ninja',
            '-DCMAKE_BUILD_TYPE=RelWithDebInfo', '-DCMAKE_POLICY_VERSION_MINIMUM=3.5',
            "-DCMAKE_TOOLCHAIN_FILE=$deps/scripts/buildsystems/vcpkg.cmake",
            "-DLuaJit_INCLUDE_DIR=$prefix/include/luajit", "-DLuaJit_LIBRARY=$prefix/lib/lua51.lib",
            '-DBUILD_LAUNCHER=OFF', '-DBUILD_WIZARD=OFF', '-DBUILD_OPENCS=OFF',
            '-DBUILD_MWINIIMPORTER=OFF', '-DBUILD_ESSIMPORTER=OFF', '-DBUILD_BSATOOL=OFF',
            '-DBUILD_NIFTEST=OFF', '-DBUILD_NAVMESHTOOL=OFF', '-DBUILD_BULLETOBJECTTOOL=OFF',
            '-DBUILD_COMPONENTS_TESTS=ON', '-DBUILD_OPENMW_TESTS=ON',
            '-DOPENMW_USE_SYSTEM_BULLET=OFF', '-DOPENMW_USE_SYSTEM_MYGUI=OFF',
            '-DUSE_MSVC_DISABLE_RTTI=OFF',
            '-DOPENMW_USE_SYSTEM_SQLITE3=OFF', '-DOPENMW_USE_SYSTEM_YAML_CPP=OFF',
            '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON')
    }
    Build {
        Invoke-Checked $cmake (@('--build', $build, '--target') + $Targets + @('--parallel', "$Jobs"))
    }
    Test {
        if (!(Test-Path $python)) { throw 'Install Python 3.12 to verify test inventories.' }
        $evidence = Join-Path $build ('checks-' + [guid]::NewGuid().ToString('N'))
        New-Item -ItemType Directory $evidence | Out-Null
        function Get-SourceState {
            $code = 'import json,sys; from pathlib import Path; sys.path.insert(0,sys.argv[1]); from run_checks import source_state; print(json.dumps(source_state(Path(sys.argv[2]),Path(sys.argv[3]))))'
            $state = & $python -B -c $code "$repo/.codex/skills/openmw-build-test/scripts" $repo $evidence
            if ($LASTEXITCODE -ne 0) { throw 'Could not fingerprint the source tree.' }
            return $state
        }
        $before = Get-SourceState
        $before | Write-Utf8 "$evidence/source-before.json"
        Copy-Item "$build/CMakeCache.txt" "$evidence/CMakeCache.txt"
        foreach ($name in $TestBinaries) {
            if ($name -notin @('components-tests', 'openmw-tests')) { throw "Unknown test binary: $name" }
        }
        Invoke-Checked $cmake (@('--build', $build, '--target') + $TestBinaries + @('--parallel', "$Jobs"))
        foreach ($key in @([Environment]::GetEnvironmentVariables().Keys)) {
            if ($key -like 'GTEST_*') { [Environment]::SetEnvironmentVariable($key, $null, 'Process') }
        }
        Write-Host "Test evidence: $evidence"
        foreach ($name in $TestBinaries) {
            & "$build/$name.exe" --gtest_list_tests | Write-Utf8 "$evidence/$name-inventory.txt"
            if ($LASTEXITCODE -ne 0) { throw 'Could not enumerate tests.' }
            Invoke-Checked "$build/$name.exe" @("--gtest_filter=$Filter", '--gtest_repeat=1',
                '--gtest_shuffle=0', "--gtest_output=xml:$evidence/$name.xml")
            Invoke-Checked $python @('-B', "$repo/.codex/skills/openmw-build-test/scripts/verify_gtest.py",
                '--inventory', "$evidence/$name-inventory.txt", '--xml', "$evidence/$name.xml", '--filter', $Filter)
            Get-FileHash "$build/$name.exe" -Algorithm SHA256 |
                Select-Object Path,Hash | ConvertTo-Json | Write-Utf8 "$evidence/$name-sha256.json"
        }
        $after = Get-SourceState
        $after | Write-Utf8 "$evidence/source-after.json"
        if ($before -ne $after) { throw 'Source changed during tests; this run does not validate one source revision.' }
    }
    Play {
        if (!(Test-Path "$OblivionData/Oblivion.esm")) { throw 'Oblivion.esm is missing.' }
        if ($OblivionData.Contains('"') -or $OblivionData.Contains("`n") -or $OblivionData.Contains("`r")) {
            throw 'Game-data paths must not contain quotes or newlines.'
        }
        $profile = Join-Path $build ('tryout-' + [guid]::NewGuid().ToString('N'))
        New-Item -ItemType Directory "$profile/user", "$profile/local-data" -Force | Out-Null
        foreach ($key in @([Environment]::GetEnvironmentVariables().Keys)) {
            if ($key -like 'OPENMW_*' -or $key -like 'SDL_*') {
                [Environment]::SetEnvironmentVariable($key, $null, 'Process')
            }
        }
        @"
replace=config
resources="$build/resources"
data="$OblivionData"
data-local="$profile/local-data"
content=Oblivion.esm
start=ImperialDungeon01
skip-menu=1
new-game=0
no-grab=1
user-data="$profile/user"
"@ | Write-Utf8 "$profile/openmw.cfg"
        "[Video]`nfullscreen = false`nresolution x = 1280`nresolution y = 720`n[Models]`nload unsupported nif files = true" |
            Write-Utf8 "$profile/settings.cfg"
        Write-Host "Private tryout profile: $profile"
        Invoke-Checked "$build/openmw.exe" @('--replace=config', '--config', $profile, '--game-profile=oblivion')
    }
}
