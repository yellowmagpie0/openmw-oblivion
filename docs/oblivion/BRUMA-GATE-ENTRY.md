# Entering the Bruma Gate

Start at the [meeting checkpoint](BRUMA-GATE-MEETING.md) and tell Burd “Let's go.” He moves in front of the guards, gives six lines of orders, and receives three replies. Bor and Soren accompany him. The other three guards take positions outside. Stay with the advancing detail while it fights the Daedra on the approach.

The first playthrough stalled on the first line. Scripted NPC conversations now play the speaker's original voice and lip animation and follow the next eligible dialogue link when the line finishes. Immediate result scripts advance the speech counter at the start of the line. Saving during a line preserves the conversation's progress. A fresh process continues at the next link. The same playthrough exposed the gate's rejected creature list. Scripted spawns now keep both the selected actor and the root leveled list's script.

The party's next objective inside is to reach the sigil stone. Stage 35 has an authored journal entry conditioned on the same quest being below 35. In the original 1.2.0416 executable (SHA-256 `a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`), `0x529B0E` stores the new high stage before `0x529B14` calls stage-entry processing. The condition's GetStage callback calls `0x529750` to read that same byte. The fork consequently records reaching 35 with no visible entry text.

The initial speech stall is recorded in `build-windows/sponsor-burdadvance01`. The full spoken course and original mid-speech checkpoint are in `sponsor-burdadvance03`. Entering ahead of Bor and Soren while they were fighting a live scamp left their combat suspended outside. In `sponsor-burdadvance05` the Knight resumed during the speech, helped the detail fight on the approach and entered behind all three men. Three stunted scamps spawned. The Knight healed and rested for one hour inside. All six guards survived.

The checkpoint and its cold reload put the Knight at 113/113 health with the three advancing guards at distinct positions in the Plane of Oblivion. The three rear guards remain outside Bruma. The stock-controls run, screenshot and frame times are in `build-windows/sponsor-burdadvance-stock01`. At 1280 × 720 with a 30 FPS cap, the 20-second sample at the entrance averaged 20.6 FPS over 414 frames, with a 95th-percentile frame time of 67.2 ms. The game executable SHA-256 is `92776c24d96deca68b18297d55b3c4bc2108179d6452b24531067b8edc29d08e`.

Run `build-windows/Continue-Inside-the-Bruma-Gate.cmd` to resume the advance on foot. The checkpoint verification reads the native save state, checks the accompanying guards against the rear guard and checks the order of the original voice files.

```powershell
$saves = 'build-windows/temple-player-ready/checkpoints'
$trip = 'build-windows/sponsor-burdadvance03'
python scripts/verify_bruma_gate_entry.py "$saves/Bruma-Gate-Met-Captain-Burd.omwsave" "$trip/MidSpeech.omwsave" "$saves/Bruma-Gate-Inside-With-Burd.omwsave" build-windows/sponsor-burdadvance-stock01/Reloaded.omwsave "$trip/openmw.log" build-windows/sponsor-burdadvance05/openmw.log
```
