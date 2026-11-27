# Meeting Captain Burd at the Bruma Gate

The Knight begins beside Martin after [giving him Azura's Star](MARTINS-ARTIFACT-HANDOFF.md). Speak to Jauffre in the Great Hall and ask where the gate is. Descend the central stairs and follow the road east and south toward Bruma. Burd leaves his guard detail and meets the Knight on the approach.

Burd offers to lead the way. The checkpoint takes “Hold on. I'm not ready yet.” and leaves *Bruma Gate* at stage 30. Resume with `build-windows/Continue-After-Meeting-Burd-at-the-Bruma-Gate.cmd`. Speak to Burd again to start his advance.

The [gate entry](BRUMA-GATE-ENTRY.md) continues through his speech, the fight on the approach and the crossing.

In the first run, enabling the gate and its guard detail in Jauffre's result script paused the conversation. From the stage-20 command to his first word took 2.052 seconds. Updating the changed enable-parent trees brought that interval down to 0.141 seconds on the same save. The gate's object script also stopped at its worldspace check every frame. That query now compares the reference's current exterior worldspace with the requested worldspace.

The original run is in `build-windows/sponsor-brumagate01`. In `sponsor-brumagate02`, the gate played its idle, the storm began inside the 4,000-unit approach radius, and talking to Burd a second time yielded both choices again. The final checkpoint in `sponsor-brumagate03` looks down the road past the guards to the gate.

At the checkpoint, 141 of 330 main-thread samples included the leveled-actor placement update. Unresolved creatures were repeatedly looking up enable parents across the same saved references. Sharing one index across those queries raised the 20-second gate sample from 10.5 to 26.1 FPS at 1280 × 720 with a 30 FPS cap. The 95th-percentile frame time fell from 146.5 to 54.0 ms. The first sample overlapped other verification work. The call stacks are in `sponsor-brumagate-prof03/gate-profile.json`. The final stock-controls run, screenshot and frame times are in `sponsor-brumagate-stock02`. The game executable SHA-256 is `0e87223dc467c000ae988caf9713272ad1872bf4ac1f2a44253580897674d112`.

```powershell
$saves = 'build-windows/temple-player-ready/checkpoints'
$trip = 'build-windows/sponsor-brumagate02'
python scripts/verify_bruma_gate_meeting.py "$saves/Azuras-Star-Delivered.omwsave" "$trip/Assigned.omwsave" "$saves/Bruma-Gate-Met-Captain-Burd.omwsave" build-windows/sponsor-brumagate-stock02/Reloaded.omwsave "$trip/openmw.log"
```
