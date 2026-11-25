# Tar-Meena's cipher

From the [fourth-volume save](SPONSOR-MEETING.md), leave the Sponsor's meeting room, return to the Arcane University and ask Tar-Meena about the Mythic Dawn. Return on two subsequent days with all four Commentaries. On the third visit, tell her you have the last two volumes. She decodes the first letters of the paragraphs and directs you to Green Emperor Way at noon.

Run `build-windows/Continue-After-Tar-Meenas-Clue.cmd` to resume at the Arch-Mage's Lobby exit. The checkpoint is `build-windows/temple-player-ready/checkpoints/Path-of-Dawn-Green-Emperor-Way.omwsave`. Path of Dawn is at stage 93. The player has all four books and the Sunken Sewers key. The next chunk starts with finding the Tomb of Prince Camarril and observing the noon inscription.

## Evidence

The winning Oblivion master (SHA-256 `a26e21ea8c3041f8737ffb3a266129dedb7f8a88590625ecfecd5eb7f66b4a70`) supplies dialogue INFOs `024e6c`, `024e6b`, `022b3f` and `024e81`. Quest stages 91 and 92 each record the current day. The stage 91 result also calls AddItem with a count of −1 for each of Tar-Meena's first two volumes. The NPC's physical inventory presents one copy of each. Save format 109 remembers the signed stock for subsequent script changes. The stage 93 target is the placed tomb activator `024e4a`.

`build-windows/sponsor-clues-final05` records the playthrough from the fourth-volume checkpoint on game SHA-256 `337ba5e5d55daf317c44b7ebfb82738bb283921315a9f344bc448261075823cc`. The player climbed the sewer stairs, used the ladder to Fathis Ules' basement, exited through his house, fast-travelled to the University and entered the lobby. Both 24-hour waits used the normal Wait menu. The final dialogue played all three voiced responses with their lip tracks. The player then walked to the lobby exit and saved. The walking aid supplied movement and camera input. Quest stages, books and travel came from game actions.

```powershell
python scripts/verify_tar_meena_cipher.py build-windows/temple-player-ready/checkpoints/Path-of-Dawn-Green-Emperor-Way.omwsave --first-hint build-windows/sponsor-clues-final05/First-hint.omwsave --course-log build-windows/sponsor-clues-final05/openmw.log --restart-save build-windows/sponsor-clues-stock15/Reloaded.omwsave
```

The sewer compass now checks which passages connect to the player's landing and ranks equal-door exits by walking distance. The playback in `sponsor-clues-ladder11` reached the Fathis Ules ladder with the compass at zero metres and used it to leave. `Sewer-compass.png` captures the marker and activation prompt together. The final dialogue is pictured in `sponsor-clues-final05/Tar-Meenas-cipher.png`. See the [native inventory provenance](M15-NATIVE-RULE-PROVENANCE.md) for the signed AddItem rule.

The stock-controls restart in `build-windows/sponsor-clues-stock15` used game SHA-256 `cafacc17a56da46b92ecbd0d669561ccbbed47645e0955b1d6049440ecba4bec`. At 1280 × 720 with a 30 FPS cap, the stationary lobby view averaged 27.0 FPS over 676 frames in seconds 15–40 after gameplay began. The 95th-percentile frame took 47.8 ms and the 99th took 56.4 ms. The same checkpoint previously averaged 17.3 FPS with a 1,352 ms 99th percentile (`sponsor-clues-stock08`). The route search now skips doors that cannot improve the route and resolves the actor's factions once per search.

The restart displayed “Tomb of Prince Camarril” above the lobby exit, retained both NPC study copies on resave, and accepted the door to the University grounds. The checkpoint SHA-256 is `41a4524bd7ec7bb77f352c3411868282909a5a2fbecca2625df59293508caaea`. The full Windows inventories passed with 2,357 engine tests and 3,328 component tests. Results and the tested source fingerprint are in `build-windows/checks-37661c8fd91a4e8fae326f72cb1d8159`. All 205 Python runtime-state tests passed.
