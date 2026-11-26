# Returning the Mysterium Xarxes to Martin

Run `build-windows/Continue-After-Giving-Martin-the-Xarxes.cmd` to resume at Cloud Ruler Temple. Martin has the book and asks the player to speak with Jauffre about reports of spies in Bruma.

The [spy briefings](SPIES-BRIEFING.md) continue this save through Steffan's sighting.

From the [Lake Arrius checkpoint](DAGON-SHRINE-ESCAPE.md), use the world map to fast travel to the discovered temple. Walk up the stairs, enter the Great Hall and take the east wing door. Martin was in the east wing at the arrival time of this playthrough. Tell him you have the Xarxes, then ask whether it can lead you to Camoran. The last response starts Spies. The original quest enables a study copy of the book in the Great Hall.

## Runtime and content checks

The original master, identified in the preceding handoff, defines the removal in dialogue response `08b618`. Response `08b619` completes MQ06 and starts MQ07 at stage 10. The latter enables the Great Hall reference `08b607`, grants Jeelius clothing if he survived, and attempts to remove the book once more. The second removal finds zero copies. The two quest results together award one point of Fame.

Two ordinary quicksaves exposed independent failures. Fast travel expired timed cult equipment while its owner still had an inventory in an inactive cell store. After Martin accepted the Xarxes, the removed copy still owned script locals. Expiry now updates the resident inventory and its effect owners together. Destructive inventory removal retires the depleted instance, deferring retirement while a script frame is active. The quest marker also follows scheduled actors' simulated positions until they materialize in their new cell.

At night the initial playthrough repeatedly moved the Forlorn Watchman every frame. The master’s compiled condition in `MS06Script` (`0347c7`) groups the two time checks before testing the enable flag. The opposite ordering in `MS04DoorScript` (`03db37`) also evaluates OR first. The parser now follows both compiled examples. The new VM test covers both operand orders and explicit parentheses.

The replay uses movement and camera steering in an isolated profile at normal difficulty. Map travel, doors, dialogue choices, voice playback and saving use the game interfaces. The playable launcher uses stock controls. Recheck the inventory, quest stages, study copy, Fame and Jeelius with the command below.

```powershell
$run = 'build-windows/sponsor-martin-handoff05'
$save = 'build-windows/temple-player-ready/checkpoints/Mysterium-Xarxes-Delivered.omwsave'
python scripts/verify_martin_xarxes.py "$run/BeforeMartin.omwsave" $save --restart build-windows/sponsor-martin-stock01/Reloaded.omwsave --log "$run/openmw.log"
```

The final replay is `sponsor-martin-handoff05`. After the handoff Martin walked to the Great Hall and sat at the study table. `XarxesInTheHall.png` shows the book under the crosshair and the HUD pointing to Jauffre. The nighttime Watchman script issued one move during the full journey. The interval between the first MQ06 completion event and Martin's final response appearing was 1.15 seconds. A separately launched stock process loaded the handoff, resaved it and passed the same state checks.

The checkpoint SHA-256 is `c70d4eba89e35b3d7b9791c2bf7391db1e40a9588302c024974056044123582e`. The Windows inventories in `build-windows/checks-fc11c0bb5fe544739396e4ca6af27418` passed with 2,363 engine tests and 3,331 component tests. All 212 portable Python tests passed.
