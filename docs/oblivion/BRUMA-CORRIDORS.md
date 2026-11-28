# Through the Corridors of Dark Salvation

The high balcony checkpoint is `build-windows/Continue-On-the-Fury-Spike-High-Balcony.cmd`. The Knight has 113/113 health and faces up the tower ramp. Burd, Bor and Soren are on the balcony. Bruma Gate is at stage 35. Continue [past the Sigil Keeper into the chamber](#sigil-keeper-and-chamber-entrance).

From the [corridor entrance](BRUMA-HALL-ASCENT.md#upper-tower-climb), the player walked down the passage, took the southern branch and climbed both ramps. All three guards defended the player against the melee Dremora Churl on the lower climb and the mage Churl on the upper climb. Burd finished both. The party regrouped in the upper passage before taking door `0562AB` back into the Fury Spike. The player stepped away from the door, healed and sheathed their sword.

The playthrough used scripted movement and combat controls. The two landing saves record the party's location and each Churl's state. The full-resolution frames in `build-windows/sponsor-corridors-climb02` include the party outside the upper exit (`GuardsAtExit.png`) and the view up the tower ramp (`Ready.png`). At 1280 × 720 with a 30 FPS cap, the two stretches of movement and combat averaged 27.8 FPS across 55 seconds, with a 95th-percentile frame time of 49.5 ms. The viewer samples cover seconds 26–53 and 61–89 of the playthrough, excluding the intermediate quicksave.

A fresh process loaded the high-balcony checkpoint with normal controls. The player and all three guards remained on the balcony at distinct positions, and both Churls remained dead. The 20-second balcony sample averaged 27.5 FPS, with a 95th-percentile frame time of 49.8 ms. The reload frame and sample are in `build-windows/sponsor-corridors-climb-stock01`.

```powershell
$b = 'build-windows'
$saves = "$b/temple-player-ready/checkpoints"
$run = "$b/sponsor-corridors-climb02"
python scripts/verify_bruma_corridors.py "$saves/Bruma-Gate-Corridors-of-Dark-Salvation.omwsave" "$run/MiddleLanding.omwsave" "$run/UpperLanding.omwsave" "$run/openmw.log" "$saves/Bruma-Gate-Fury-Spike-High-Balcony.omwsave" "$b/sponsor-corridors-climb-stock01/Reloaded.omwsave"
```

## Sigil Keeper and chamber entrance

Resume with `build-windows/Continue-In-the-Sigillum-Sanguis.cmd`. The next leg climbs the chamber to the Sigil Stone. The player has 113/113 health and one Sigil Key. Burd, Bor and Soren are alive at the foot of the ramp, and the Bruma Gate journal has reached stage 40.

The player ran up from the high balcony, fought the Keeper with the three guards and landed the final hit. The player took the key from the body, opened the level-40 door `05622A`, and stepped clear of the chamber entrance. The other tower door still had its lock. The playthrough, loot menu and keyed crossing are in `build-windows/sponsor-keeper01`. After entry the movement script tried to read combat stats from a leveled spawn marker. The corrected controls in `sponsor-keeper-landing02` resumed from the arrival save and walked up the ramp.

The chamber uses the exterior weather system and specifies `ClimateSigil`. Sixteen fixed-camera frames in `sponsor-keeper-compare01` show the lightning brightening the ramp and receding. On the previous fork build the climate settings were applied at the door and skipped when loading directly into the chamber. The weather manager now tracks which climate it has applied separately from the saved region, so loading the save configures the sky and day length while preserving any weather transition.

```powershell
$b = 'build-windows'
$saves = "$b/temple-player-ready/checkpoints"
$run = "$b/sponsor-keeper01"
$cold = "$b/sponsor-keeper-stock02"
python scripts/verify_bruma_sigil_keeper.py "$saves/Bruma-Gate-Fury-Spike-High-Balcony.omwsave" "$run/KeyTaken.omwsave" "$run/openmw.log" "$saves/Bruma-Gate-Sigillum-Sanguis.omwsave" "$cold/Reloaded.omwsave" "$cold/openmw.log"
```

Both normal-controls reloads with the rebuilt executable (SHA-256 `8889a091c9147c4d0e1de2709946127ad3933efd5609eddcfadd960c0fe622b1`) applied the chamber climate. The lightning and the darker intervals are visible in `sponsor-keeper-stock01/Chamber00.png` through `Chamber07.png`. After the build and tests finished, the second reload averaged 25.0 FPS over 20 seconds at 1280 × 720 and a 30 FPS cap, with a 95th-percentile frame time of 58.7 ms. All 2,368 engine tests passed, including the climate restore regression. The test inventory and results are in `build-windows/checks-0c57eadda85240509a2c7e6482bca5a8`.
