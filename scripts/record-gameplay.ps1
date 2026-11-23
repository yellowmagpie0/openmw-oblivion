# Portable, offline recording. Does not start, configure, or build OpenMW.
[CmdletBinding()]
param([ValidateSet('Start', 'Setup', 'Videos')][string]$Action = 'Start')

$ErrorActionPreference = 'Stop'
$workspace = Split-Path -Parent $PSScriptRoot
$recorderRoot = Join-Path $workspace 'deps\gameplay-recorder'
$obsRoot = Join-Path $recorderRoot 'obs'
$obsExe = Join-Path $obsRoot 'bin\64bit\obs64.exe'
$configRoot = Join-Path $obsRoot 'config\obs-studio'
$videoRoot = Join-Path $recorderRoot 'videos'
$profileRoot = Join-Path $configRoot 'basic\profiles\Travel'
$sceneRoot = Join-Path $configRoot 'basic\scenes'
$version = '33.0.1'
$sha256 = '0d9436451c2d772b33428e3fdae9776f071895c335de17283176306fb72651d3'

function Write-Utf8([string]$Path, [string]$Text) {
    [IO.File]::WriteAllText($Path, $Text, [Text.UTF8Encoding]::new($false))
}

New-Item -ItemType Directory -Force $videoRoot | Out-Null
if ($Action -eq 'Videos') {
    Start-Process explorer.exe -ArgumentList ('"' + $videoRoot + '"')
    exit 0
}

if (!(Test-Path -LiteralPath $obsExe)) {
    if ($Action -ne 'Setup') {
        throw 'Recorder is missing. Run scripts\record-gameplay.ps1 -Action Setup while online.'
    }
    New-Item -ItemType Directory -Force $recorderRoot | Out-Null
    $archive = Join-Path $recorderRoot "OBS-Studio-$version-Windows-x64.zip"
    if (!(Test-Path -LiteralPath $archive)) {
        [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
        Invoke-WebRequest "https://github.com/obsproject/obs-studio/releases/download/$version/OBS-Studio-$version-Windows-x64.zip" -OutFile $archive -UseBasicParsing
    }
    if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant() -ne $sha256) {
        throw 'OBS archive checksum mismatch. No downloaded executable was started.'
    }
    Expand-Archive -LiteralPath $archive -DestinationPath $obsRoot -Force
    if ((Get-AuthenticodeSignature -LiteralPath $obsExe).Status -ne 'Valid') {
        throw 'OBS executable signature validation failed.'
    }
}

# Only initialize our own profile once; keep later changes made in OBS.
if (!(Test-Path -LiteralPath (Join-Path $profileRoot 'basic.ini'))) {
    $running = Get-Process obs64 -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $obsExe }
    if ($running) { throw 'Close this portable OBS before initializing its profile.' }
    New-Item -ItemType Directory -Force $profileRoot, $sceneRoot | Out-Null
    Write-Utf8 (Join-Path $obsRoot 'portable_mode.txt') ''
    Write-Utf8 (Join-Path $configRoot 'global.ini') "[General]`nEnableAutoUpdates=false`n"
    Write-Utf8 (Join-Path $configRoot 'user.ini') @'
[General]
FirstRun=true
HotkeyFocusType=NeverDisableHotkeys
ConfirmOnExit=true
ProcessPriority=BelowNormal

[Basic]
Profile=Travel
ProfileDir=Travel
SceneCollection=Travel
SceneCollectionFile=Travel.json
ConfigOnNewProfile=false

[BasicWindow]
PreviewEnabled=false
SysTrayEnabled=true
SysTrayWhenStarted=false
StudioMode=false
'@
    # OBS INI strings escape Windows path separators.
    $iniVideoPath = $videoRoot.Replace('\', '\\')
    $profileText = @"
[General]
Name=Travel

[Output]
Mode=Advanced
FilenameFormatting=OpenMW-%CCYY-%MM-%DD_%hh-%mm-%ss

[AdvOut]
RecType=Standard
RecFilePath=$iniVideoPath
RecFormat2=hybrid_mp4
RecEncoder=obs_qsv11_v2
RecAudioEncoder=ffmpeg_aac
RecTracks=1
RecUseRescale=false
RecRB=false
Track1Bitrate=96
Track1Name=Desktop sound

[Video]
BaseCX=1280
BaseCY=720
OutputCX=1280
OutputCY=720
FPSType=0
FPSCommon=30
ScaleType=bilinear
ColorFormat=NV12
ColorSpace=709
ColorRange=Partial

[Audio]
SampleRate=48000
ChannelSetup=Stereo

[Hotkeys]
OBSBasic.StartRecording={"bindings":[{"control":true,"shift":true,"key":"OBS_KEY_F8"}]}
OBSBasic.StopRecording={"bindings":[{"control":true,"shift":true,"key":"OBS_KEY_F8"}]}
"@
    Write-Utf8 (Join-Path $profileRoot 'recordEncoder.json') @'
{"target_usage":"TU7","rate_control":"CBR","bitrate":2500,"keyint_sec":2,"profile":"high","latency":"normal","bframes":2}
'@
    $sourceId = [guid]::NewGuid().ToString()
    $sceneId = [guid]::NewGuid().ToString()
    $scene = @{
        name = 'Travel'
        current_scene = 'OpenMW gameplay'
        current_program_scene = 'OpenMW gameplay'
        scene_order = @(@{ name = 'OpenMW gameplay' })
        sources = @(
            @{
                name = 'OpenMW only'; uuid = $sourceId
                id = 'game_capture'; versioned_id = 'game_capture'
                settings = @{
                    capture_mode = 'window'; window = 'OpenMW:SDL_app:openmw.exe'
                    priority = 2; limit_framerate = $true; capture_cursor = $true
                    capture_overlays = $false; anti_cheat_hook = $false
                }
                enabled = $true; mixers = 0; volume = 1.0
            },
            @{
                name = 'OpenMW gameplay'; uuid = $sceneId; id = 'scene'; versioned_id = 'scene'
                settings = @{
                    id_counter = 1
                    items = @(@{
                        name = 'OpenMW only'; source_uuid = $sourceId; id = 1
                        visible = $true; locked = $true
                        pos = @{ x = 0.0; y = 0.0 }; scale = @{ x = 1.0; y = 1.0 }
                        rot = 0.0; align = 5
                        bounds_type = 2; bounds_align = 0
                        bounds = @{ x = 1280.0; y = 720.0 }
                    })
                }
            }
        )
        DesktopAudioDevice1 = @{
            name = 'Desktop sound'; uuid = [guid]::NewGuid().ToString()
            id = 'wasapi_output_capture'; versioned_id = 'wasapi_output_capture'
            settings = @{ device_id = 'default' }
            mixers = 1; volume = 1.0; muted = $false; enabled = $true
        }
        current_transition = 'Cut'; transition_duration = 0
        transitions = @(); groups = @(); quick_transitions = @()
        modules = @{}; version = 2
    }
    Write-Utf8 (Join-Path $sceneRoot 'Travel.json') ($scene | ConvertTo-Json -Depth 15)
    # Write the profile last, so failed setup can be retried.
    Write-Utf8 (Join-Path $profileRoot 'basic.ini') $profileText
}

if ($Action -eq 'Setup') {
    Write-Output "Ready for offline recording. Videos: $videoRoot"
    exit 0
}

if (Get-Process obs64 -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $obsExe }) {
    Write-Output 'The portable recorder is already open. Use its taskbar icon.'
    exit 0
}
Write-Output 'OpenMW recorder: Ctrl+Shift+F8 starts/stops recording. Microphone is off.'
Write-Output "Videos: $videoRoot"
Start-Process -FilePath $obsExe -WorkingDirectory (Split-Path $obsExe) -ArgumentList '--portable', '--disable-updater', '--only-bundled-plugins', '--profile', 'Travel', '--collection', 'Travel' -WindowStyle Normal
