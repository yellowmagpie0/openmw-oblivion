# From the Fury Spike to the Rending Halls

Continue from this checkpoint [through the Rending Halls to the upper Fury Spike balcony](BRUMA-HALL-ASCENT.md).

From the [Fury Spike entrance](BRUMA-GATE-TOWER.md), fight the two Dremora on the lower floor with Burd, Bor and Soren. Take the north door to the Rending Halls. The new checkpoint is at the foot of the curved ramp after a one-hour rest. Resume with `build-windows/Continue-In-the-Rending-Halls.cmd`. All three guards are alive, the Knight has 113/113 health, and Bruma Gate remains at stage 35. The next leg follows the ramp up through the halls toward the sigil stone.

On the first run all three guards followed the player past the attackers. Both Dremora use NPC records, and the existing defense check only considered creature attackers. An NPC following the player directly or through a leader can now defend against an independently hostile NPC targeting the player. The observer still needs to detect the attacker and pass the usual fight-score check. The friend-disposition term is documented in the [native rule audit](M15-NATIVE-RULE-PROVENANCE.md). The follower eligibility rule was tested in this fork.

On replay all three guards acquired both Dremora. One of Burd's men killed the first, and Burd killed the second. All three crossed the door and followed the player down the entrance passage to the ramp.

## Reproduce the checkpoint check

The first run is in `build-windows/sponsor-rending01`, the replay in `sponsor-rending02`, and the fresh normal-controls load in `sponsor-rending-stock01`. The latter contains the screenshot, frame sample and a second save. The acceptance check reads the game log and saved actor, quest and position records.

```powershell
$b = 'build-windows'
$saves = "$b/temple-player-ready/checkpoints"
python scripts/verify_bruma_rending_halls.py "$saves/Bruma-Gate-The-Fury-Spike.omwsave" "$b/sponsor-rending02/openmw.log" "$saves/Bruma-Gate-Rending-Halls.omwsave" "$b/sponsor-rending-stock01/Reloaded.omwsave"
```
