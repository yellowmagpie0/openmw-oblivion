# Through the Rending Halls

Start at the [foot of the first ramp](BRUMA-RENDING-HALLS.md). Burd and his men follow up to the first chamber, where the party encounters three enemies. Open the animated gate on the west side, climb the second ramp and cross the blade corridor. After the fourth fight, take the east door on the upper landing back to the Fury Spike.

The balcony checkpoint is `build-windows/Continue-On-the-Fury-Spike-Balcony.cmd`. Burd, Bor and Soren are on the balcony, the Knight has 113/113 health after resting, and Bruma Gate is at stage 35. The [upper tower climb](#upper-tower-climb) continues from here.

## Corridor trap

On the first crossing, the blade stayed idle. The installed master links actor trigger `165C0A` to blade `056251`. Its `TrigZoneACTORRESET02SCRIPT` activates its parent in an `OnTriggerActor` block. Actor overlaps now dispatch that handler. On the first replay the latch fired, but repeated screenshots showed the blade still suspended overhead.

The original model animates the latch and suspends the blade from a chain of physics bodies. For Oblivion meshes with an `Unequip` sequence and a convex damage phantom on a constrained chain leading to a fixed body, the fork approximates the taut chain as a damped pendulum with a separately hinged cutting head. The animation holds the chain until the latch releases, then rotates it at its upper link. The blade mesh and its damage volume follow the same motion. The track is calculated once when the mesh loads, and the existing animation progress is saved with the reference. The bind transforms and damage hull determine the swing plane, chain length and head offset. Authored tracks on the moving bodies take precedence.

## Runtime evidence

The generalized swing was replayed in three fresh game processes using the Windows executable with SHA-256 `fbc39bacf4c1caf050aae374adf4a305b3a345e5df62c972147288e16e42dcd3`. From the pre-blade checkpoint, `build-windows/sponsor-general-blade-swing03` captures the moving head and the player reaching the far side after waiting. In `sponsor-general-blade-hit03` the first blade hit reduced player health from 94.86 to 67.86. The impact log identifies blade body 69 and the player, and the Impact save records the resulting health. In `sponsor-general-blade-resume03` a cold load of the new MidSwing save advanced from 12.7% to 13.7% on the first capture and eventually reached 100%. The resumed and uninterrupted Rest frames show the same hanging pose. Each profile used scripted movement and the engine's save menu API to capture game state and rendered frames.

Under `build-windows`, the original crossing is in `sponsor-hall-ascent01` and `sponsor-hall-ascent02`. The actor-trigger replay is in `sponsor-hall-ascent03` and the stationary-blade frames in `sponsor-hall-blade01`. The moving chain is visible in `sponsor-hall-ascent04`. The replay in `sponsor-hall-ascent05` starts from the original ramp save. A direct crossing took 27 damage per blade hit and killed the player. The successful party resumed from its pre-blade save in `sponsor-hall-ascent06`, let the swing pass, and took the upper door. The two logs record the gate, four enemy deaths and the upper exit. Compare `BeforeBlade.png`, `Blade.png` and `BladeLater.png` for the swing. The mid-swing save reached the hanging rest pose after a cold load in `sponsor-hall-blade-resume02`. The normal-controls balcony reload in `sponsor-hall-ascent-stock02` averaged 27.0 FPS over 20 seconds at 1280 × 720 and a 30 FPS cap. The 95th-percentile frame time was 54.0 ms.

```powershell
$b = 'build-windows'
$saves = "$b/temple-player-ready/checkpoints"
$approach = "$b/sponsor-hall-ascent05"
$run = "$b/sponsor-hall-ascent06"
python scripts/verify_bruma_hall_ascent.py "$saves/Bruma-Gate-Rending-Halls.omwsave" "$approach/openmw.log" "$approach/BeforeBlade.omwsave" "$run/openmw.log" "$run/Blade.omwsave" "$run/UpperExit.omwsave" "$saves/Bruma-Gate-Fury-Spike-Balcony.omwsave" "$b/sponsor-hall-ascent-stock02/Reloaded.omwsave"
```

## Upper tower climb

Resume with `build-windows/Continue-In-the-Corridors-of-Dark-Salvation.cmd`. The Knight is at 113/113 health, facing down the entrance passage. Continue [through the southern ramps to the high Fury Spike balcony](BRUMA-CORRIDORS.md).

From the balcony save, the player ran up the curved ramp with Burd, Bor and Soren. Two stunted scamps engaged the party, one on the incline and one near the upper landing. Burd killed both. The landing save places all three guards within 270 game units of the player before the door opened. Door `056232` led into the Corridors of Dark Salvation, and all three guards were visible in the new cell about six seconds after activation. The player moved clear of the entrance, healed and saved. A fresh launch with normal controls restored the three guards at separate positions and Bruma Gate at stage 35.

The playthrough used the same executable as the generalized blade replay above. At 1280 × 720 with a 30 FPS cap, the climb and fights averaged 26.3 FPS over 27 seconds, with a 95th-percentile frame time of 56.5 ms. The stationary entrance view after reload averaged 28.2 FPS over 20 seconds, with a 95th-percentile frame time of 47.1 ms. The playthrough profile contains `Landing.png` and `Arrival.png`. The normal-controls profile contains `Checkpoint.png`.

```powershell
$b = 'build-windows'
$saves = "$b/temple-player-ready/checkpoints"
$run = "$b/sponsor-upper-tower01"
python scripts/verify_bruma_upper_tower.py "$saves/Bruma-Gate-Fury-Spike-Balcony.omwsave" "$run/Landing.omwsave" "$run/openmw.log" "$saves/Bruma-Gate-Corridors-of-Dark-Salvation.omwsave" "$b/sponsor-upper-tower-stock01/Reloaded.omwsave"
```
