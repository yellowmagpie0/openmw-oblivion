# Isolated original-game probes

Use original-game normal input when the case needs actual gameplay evidence.
Emulated instructions do not prove timing, collisions, UI, dispatch or restart.
Inspect the current process/window/prefix state; never reuse remembered PIDs or
window IDs. Do not kill an unrelated user's game/Steam process.

The established isolated Proton prefix is:
`build/oblivion-compat/m15/S2/original-06/compat`.
Its plugin list is `pfx/drive_c/users/steamuser/AppData/Local/Oblivion/Plugins.txt`;
saves/config are under `pfx/drive_c/users/steamuser/Documents/My Games/Oblivion`.
Verify and back up relevant inputs before a probe; restore the exact prior state
on exit rather than assuming the plugin list should be master-only.

Known pristine `quicksave.ess` SHA-256:
`67d927cad4a370131882da58b04a25a330c5defe288b9b6ca21f7768815c4d66`.
Never overwrite it with F5. F9 reloads a clean attempt. Test saves should use a
separate name. Compare the pristine hash after the run.

Proven launch shape, from the original game directory:

```bash
xvfb-run -a -s '-screen 0 1280x768x24' env \
  STEAM_COMPAT_DATA_PATH='ABSOLUTE_ISOLATED_COMPAT_PATH' \
  STEAM_COMPAT_CLIENT_INSTALL_PATH='/home/maciek/.local/share/Steam' \
  SteamAppId=22330 SteamGameId=22330 \
  '/home/maciek/.local/share/Steam/steamapps/common/Proton - Experimental/proton' \
  run ./Oblivion.exe
```

Use absolute log/capture paths. Read prior `original-16`/`original-17` action
scripts for focus/input handling. The root display was 1280×768 and the game
1280×720. Explicitly focus the current game window before input; loading can
return focus to root. `moveto` can close the console asynchronously: inspect the
result before sending the next console command. Tested controls are F draw,
Space activate, E jump, Tab menu and grave console. Key down/up intervals around
0.6 s and held mouse attacks around 0.3 s worked; allow settling after movement.
These timings are operational starting points, not gameplay formula evidence.

Fresh-load each independent first-hit test: even zeroed combat-style chances do
not make an NPC passive, and engagement/fatigue can change subsequent results.
For bow tests capture release-time fatigue/draw state; a nominal hold duration
alone does not prove exact release-frame damage. Define measurement tolerances
before running. The original jail elapsed-time observation demonstrated that
24.0003 game hours can be a normal frame-rounded day; a too-tight tolerance is
a failed case to retain, not erase after adjusting it.

Exit through the game where possible and record both game and wrapper status.
A post-shutdown key-up failure can differ from game failure; document it rather
than treating any available screenshot as acceptance. Keep captures/extracted
content under ignored build evidence, and preserve editable fixture manifests
and reproducible writers in Git.
