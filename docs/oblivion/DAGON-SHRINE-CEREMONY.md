# Mankar's sermon and the shrine ceremony

Run `build-windows/Continue-After-Freeing-Jeelius.cmd` to resume with the Mysterium Xarxes on the lectern and the cult hostile. To replay the decision in peace, run `build-windows/Continue-At-the-Shrine-Altar.cmd` and activate Jeelius.

The course starts at [Harrow's shrine entrance checkpoint](HARROWS-ESCORT.md). Follow Harrow down into the sanctuary. Mankar and the congregation speak, Mankar gestures from the pulpit and departs through the Paradise portal. Harrow introduces the initiate to Ruma. She asks the player to take the dagger and sacrifice Jeelius.

The playthrough accepted Ruma's instruction to remain undercover, then activated Jeelius on the altar and chose to release him. That choice turns the cult hostile. The next objective is to retrieve the Mysterium Xarxes and escape.

The [escape playthrough](DAGON-SHRINE-ESCAPE.md) continues from that release through the cavern exit at Lake Arrius.

## Gameplay evidence

The entrance save originally had Jeelius in another cell with an interrupted sleep package. His exact assigned bed is the sacrifice altar, which he owns. The low-process actor now reaches that furniture when its interior loads and the abstract route through locked doors has failed. The shrine altar's active NIF marker selects the right-hand bed entry, the lying idle and the exit. The actor stays on the marker while unconscious and can still be activated. The release script clears that state and selects a flee package with a destination marker.

The portal script originally advanced the journal even though the locked door denied Mankar passage. The door is owned by his NPC base. He can now use it while it remains locked to others. The replay verified his saved cell in Paradise, the portal closing and Ruma's spoken sacrifice instruction.

The full replay is in `build-windows/sponsor-ceremony-final04`. Movement and camera steering used a local input aid. Dialogue choices, activating Jeelius, clicking the release prompt and saving used normal game inputs. The altar save is at stage 50. The second save is at stage 51 with Jeelius conscious, over 500 game units from the altar and running toward his scripted refuge. `Jeelius.png`, `Release.png`, `Awake.png` and `Leaving.png` show the transition. A separate sample at 1280 × 720 with a 30 FPS cap averaged 27.6 FPS during the sermon, with a 52 ms 95th-percentile frame and no frames over 100 ms (profile `sponsor-ceremony02`, OSG time 25–90).

The SHA-256 of `Dagon-Shrine-At-the-Altar.omwsave` is `e2ade0b31814455cc2be0d3337c26e613caa6c93e20623089386bcdcb5cc6092`. The SHA-256 of `Dagon-Shrine-Jeelius-Freed.omwsave` is `0c9a4620ceb465a7875769758057ce7edc54ca81e5c84543288935a3a52aa599`. Both are in `build-windows/temple-player-ready/checkpoints`.

Fresh stock-control processes loaded and resaved both checkpoints. In a separate restart, Tab focused Yes and Enter confirmed the release. `build-windows/sponsor-ceremony-input10/Confirmed.png` shows Jeelius sitting up. The main playthrough selected Yes by mouse. The automated click missed the button in the stock restart. The final executable SHA-256 is `6301961c9836d9717ef389a9bc220e89f974c3228e19adaa35c208ecdfaf0800`. The full Windows inventories passed with 2,362 engine tests and 3,328 component tests in `build-windows/checks-1300f92d7c0a436ea5c1e0dee363048f`. All 212 portable Python tests passed.

```powershell
$saves = 'build-windows/temple-player-ready/checkpoints'
python scripts/verify_shrine_ceremony.py "$saves/Dagon-Shrine-At-the-Altar.omwsave" "$saves/Dagon-Shrine-Jeelius-Freed.omwsave" --altar-restart build-windows/sponsor-ceremony-altar05/Reloaded.omwsave --released-restart build-windows/sponsor-ceremony-release06/Reloaded.omwsave --log build-windows/sponsor-ceremony-final04/openmw.log
```

The original executable's `SetUnconscious` handler at `0x505AC0` accepts a nonzero value, changes the actor state and notifies the process. Clearing the flag resets only the scripted unconscious state at `0x5E8E30`. The winning Jeelius script `024dd6` sets it on load during package `024eb8` and clears it when the player releases him. That package names furniture reference `024e43`. Its base `024e09` enables bed marker 1. The placed Paradise door `0c9ec9` has lock level 100 and names Mankar as its owner. Original executable and master hashes are recorded in the [Lake Arrius handoff](LAKE-ARRIUS-CAVERNS.md#native-behavior).
