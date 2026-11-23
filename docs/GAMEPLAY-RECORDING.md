# Gameplay recording on this Windows laptop

Double-click **Record Gameplay.cmd** in the checkout root. Start the game yourself
when ready. Press **Ctrl+Shift+F8** to start recording, and press it again to stop.
The OBS **Start Recording / Stop Recording** button does the same thing. Check the
red recording indicator/timer in OBS before returning to the game. Stop recording
before closing OBS or putting the laptop to sleep.

Double-click **Open Recordings.cmd** to find the videos. They are timestamped MP4s
in `deps/gameplay-recorder/videos`, outside Git tracking. Recording works offline;
nothing streams or uploads. Copy clips somewhere safe before deleting `deps` or
moving to a different checkout. OBS's File menu also has **Show Recordings**.

The prepared Travel profile uses Intel Quick Sync H.264 hardware encoding,
1280x720 at 30 fps, a 2,500 kb/s video budget and 96 kb/s stereo desktop sound.
Allow roughly **20 MB per minute / 1.2 GB per hour**. This favors modest laptop
load and small files over pristine fast-motion detail. The speed preset is TU7;
there is no replay buffer, streaming, microphone source, or live preview. Hybrid
MP4 is OBS's interrupted-recording-resistant MP4 format. Desktop sound includes
notifications and other applications; mute those if unwanted. It follows the
default Windows playback device, including when switching to headphones.

Capture targets `openmw.exe` by executable name, not the whole desktop. Other
apps aren't substituted when the game closes. The source is fit inside a 16:9
frame; other game aspect ratios can have bars. The preview is deliberately off:
right-click it and choose **Enable Preview** briefly to inspect framing, then
turn it off again. A black preview with the game closed is expected.

If the game is black while actually running: open the **OpenMW only** source's
Properties and select the running `openmw.exe` window. Use a windowed/borderless
game if fullscreen capture fails. If Game Capture still fails, create a **Window
Capture** source for that same window using Windows Graphics Capture, hide the
Game Capture source, and fit the new source with Ctrl+F. No display capture is
configured. Actual OpenMW capture and its performance must be checked on your
first recording; setup does not launch the game.

If recording slows gameplay, first keep preview off. You can reduce OBS Settings
> Video to 20 fps. If a future driver stops exposing Quick Sync, investigate
Settings > Output > Recording; avoid switching to a CPU encoder without checking
its impact. This profile preserves your later OBS changes across launches.

The portable OBS installation and its settings live entirely under
`deps/gameplay-recorder/obs`; it does not use or overwrite another OBS profile.
Close OBS when done to release its resources. Neither launcher runs OpenMW,
changes game settings/saves, starts a build, or controls another session.

To recreate the setup on a Windows x64 machine while online:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/record-gameplay.ps1 -Action Setup
```

Setup uses the official OBS 33.0.1 portable ZIP, pinned SHA-256
`0d9436451c2d772b33428e3fdae9776f071895c335de17283176306fb72651d3`,
and checks its executable signature when extracting. An existing downloaded ZIP
can be reused offline. Normal launch never downloads anything. Existing Travel
settings are preserved; setup does not reset them.

Validated on this laptop on 2026-11-16: a synthetic six-second recording used
Quick Sync H.264 TU7 at the configured bitrate and finalized successfully.
Container inspection confirmed 180 video samples at 1280x720 plus an AAC audio
track. The clip, OBS log and verification receipt are retained locally in
`deps/gameplay-recorder/checks`. OBS was closed after the check, and the Travel
gameplay collection is selected for the next launch. Hotkey bindings were
reinspected in the saved profile; physical hotkey operation, actual game picture,
game sound, playback and gameplay performance were not tested. No game was
launched, and no engine, build or game-profile files were changed.

References: [OBS portable mode](https://obsproject.com/kb/portable-mode),
[hardware encoding](https://obsproject.com/kb/hardware-encoding), and the
[official release](https://github.com/obsproject/obs-studio/releases/tag/33.0.1).
