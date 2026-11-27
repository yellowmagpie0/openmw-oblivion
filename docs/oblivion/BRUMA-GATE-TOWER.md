# The road to the Fury Spike

Start [inside the Bruma Gate](BRUMA-GATE-ENTRY.md), head east across the broken bridge and follow the ridge north. The road turns west above the lava. The outer path skirts the claw trap on the northern rise and rejoins the climb to the Fury Spike. Enter through the main door and let Burd, Bor and Soren catch up in the entrance hall.

Resume with `build-windows/Continue-At-The-Fury-Spike.cmd`. All three companions are inside. The Knight has 113/113 health and Bruma Gate remains at stage 35. The next leg leads up the tower toward the sigil stone.

## Gameplay fixes

* At the first bridge Burd acquired a scamp as a combat target across an exterior cell boundary and stopped moving. His men waited behind him. Combat pursuit now uses the resident navmesh across neighboring cells in the same worldspace. On the replay a guard killed the scamp and the detail continued across the bridge.
* The fire towers animated but deferred their scripted Cast command. A placed activator can now fire the supported single-target fire spell at the player using the spell's projectile mesh, speed, collision and damage. On this level-two course the spell dealt six damage on an actor hit. A shot saved in flight hit terrain after a cold load. Higher-rank area spells remain outside this course.
* After a one-hour wait on the tower entrance slope, Bor and Soren could converge on nearly the same point at Burd's elevation. Accompany now uses the follower's reachable position inside the requested distance and routes toward the moving leader outside it. The two guards held separate positions on the entrance slope after waiting.

## Evidence

All directories below are under `build-windows`. The original crossing stall is in `sponsor-burdplane01`. The successful route uses the bridge from `sponsor-burdplane02`, the ridge from `sponsor-burdplane03`, the outer path and tower steps from `sponsor-burdplane04`, and the actual door crossing from `sponsor-burdplane05`. The final entrance run in `sponsor-burdplane07` resumed from that door arrival. The two ordinary guards can die on the road. On a failed attempt the claw trap and the next pair of scamps killed them. Resting between fights and taking the outer path brought both through alive.

The projectile cold load is in `sponsor-burdplane-flightcold`. The normal-controls reload, screenshot and 20-second frame sample are in `sponsor-burdplane-stock01`. The final game executable SHA-256 is `cd2cde39e0971b7463c59cce31e4663cc27bd0a1046c990ab0b3547578b655ec`.

At 1280 × 720 with a 30 FPS cap the tower entrance averaged 26.7 FPS over 537 frames, with a 95th-percentile frame time of 49.6 ms. In the final bridge replay, Burd killed the scamp across the cell boundary and all three guards reached the far side (`sponsor-burdplane-bridgefinal`). All 2,367 engine tests passed in `checks-6a5a120c163b484ab3fadeeca2378fe4`. The projectile change passed all 3,332 component tests in `checks-663105d1b3ca47128355d06b33b29f29` and all 212 Windows Python runtime and crime checks. Full Python discovery also covers an unrelated HUD fixture that requires ImageMagick's `convert`, which was unavailable on this Windows host.

```powershell
$b = 'build-windows'
$saves = "$b/temple-player-ready/checkpoints"
python scripts/verify_bruma_gate_tower.py "$saves/Bruma-Gate-Inside-With-Burd.omwsave" "$b/sponsor-burdplane02/Passage.omwsave" "$b/sponsor-burdplane03/TurretFlight.omwsave" "$b/sponsor-burdplane-flightcold/openmw.log" "$saves/Bruma-Gate-The-Fury-Spike.omwsave" "$b/sponsor-burdplane-stock01/Reloaded.omwsave"
```
