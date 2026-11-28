# Through the Rending Halls

Start at the [foot of the first ramp](BRUMA-RENDING-HALLS.md). Burd and his men follow up to the first chamber, where the party encounters three enemies. Open the animated gate on the west side, climb the second ramp and cross the blade corridor. After the fourth fight, take the east door on the upper landing back to the Fury Spike.

Resume with `build-windows/Continue-On-the-Fury-Spike-Balcony.cmd`. Burd, Bor and Soren are on the balcony, the Knight has 113/113 health after resting, and Bruma Gate is at stage 35. The next leg climbs the tower ramp toward the next hall.

## Corridor trap

On the first crossing, the blade stayed idle. The installed master links actor trigger `165C0A` to blade `056251`. Its `TrigZoneACTORRESET02SCRIPT` activates its parent in an `OnTriggerActor` block. Actor overlaps now dispatch that handler. On the first replay the latch fired, but repeated screenshots showed the blade still suspended overhead.

The original model animates the latch and suspends the blade from a chain of physics bodies. For Oblivion meshes with an `Unequip` sequence and a convex damage phantom on a constrained chain leading to a fixed body, the fork approximates the taut chain as a damped pendulum with a separately hinged cutting head. The animation holds the chain until the latch releases, then rotates it at its upper link. The blade mesh and its damage volume follow the same motion. The track is calculated once when the mesh loads, and the existing animation progress is saved with the reference. The bind transforms and damage hull determine the swing plane, chain length and head offset. Authored tracks on the moving bodies take precedence.

## Runtime evidence

Under `build-windows`, the original crossing is in `sponsor-hall-ascent01` and `sponsor-hall-ascent02`. The actor-trigger replay is in `sponsor-hall-ascent03` and the stationary-blade frames in `sponsor-hall-blade01`. The moving chain is visible in `sponsor-hall-ascent04`. The replay in `sponsor-hall-ascent05` starts from the original ramp save. A direct crossing took 27 damage per blade hit and killed the player. The successful party resumed from its pre-blade save in `sponsor-hall-ascent06`, let the swing pass, and took the upper door. The two logs record the gate, four enemy deaths and the upper exit. Compare `BeforeBlade.png`, `Blade.png` and `BladeLater.png` for the swing. The mid-swing save reached the hanging rest pose after a cold load in `sponsor-hall-blade-resume02`. The normal-controls balcony reload in `sponsor-hall-ascent-stock02` averaged 27.0 FPS over 20 seconds at 1280 × 720 and a 30 FPS cap. The 95th-percentile frame time was 54.0 ms.

```powershell
$b = 'build-windows'
$saves = "$b/temple-player-ready/checkpoints"
$approach = "$b/sponsor-hall-ascent05"
$run = "$b/sponsor-hall-ascent06"
python scripts/verify_bruma_hall_ascent.py "$saves/Bruma-Gate-Rending-Halls.omwsave" "$approach/openmw.log" "$approach/BeforeBlade.omwsave" "$run/openmw.log" "$run/Blade.omwsave" "$run/UpperExit.omwsave" "$saves/Bruma-Gate-Fury-Spike-Balcony.omwsave" "$b/sponsor-hall-ascent-stock02/Reloaded.omwsave"
```
