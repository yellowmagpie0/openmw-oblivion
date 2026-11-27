# Azura’s offering

Run `build-windows/Continue-After-Azuras-Offering.cmd` to start at Azura’s Shrine after the audience. The Knight is level 2 with 113 health. Azura’s journal entry directs the player to Gutted Mine. The map marker is revealed and the entrance is unlocked. Continue with [the five Afflicted Brethren in Gutted Mine](GUTTED-MINE.md).

## Route from the Bruma save

From the [Bruma Mages Guild checkpoint](AZURA-GLOW-DUST.md), read *The Wolf Queen* in the guild basement for one Illusion increase. Reach Bruma Caverns through Jearl’s basement. Wearing Blades armor, fight the rat there for three Heavy Armor increases. Leave the caverns and read *Immortal Blood* in J’Ghasta’s house for one Hand to Hand increase. The run left both books in place and added no crime.

Sleep in the guild basement bed and choose Strength (+3), Endurance (+4), and Personality (+2). With ten major increases earned, confirming these choices advances the Knight to level 2. Fast travel to Azura’s Shrine and wait two hours until dawn. Activate the altar and accept the offering. Azura speaks four lines before returning control. Walk back from the altar and select **Track quest** for *Azura* in the journal.

## Implementation and evidence

- A completed bed rest opens the level-up screen. The Knight portrait, attribute icons and tooltip descriptions come from Oblivion. Confirming a level updates the saved native stats and the level seen by quest scripts. The bedside replay and native health calculation are recorded in `build-windows/sponsor-level10` and `build-windows/level-health-asm.txt`.
- The altar’s script assigns a voice NPC. Dialogue selection now checks the conditions against that NPC while the altar owns playback and the result script. Before the fix all four responses were silent and the quest advanced immediately.
- Quest markers can evaluate the original death-count conditions on the five mine targets. The door's scripted unlock updates the state used to route the compass as soon as Azura finishes speaking. A manually tracked quest is saved with the character and restored on reload.

The verifier checks the starting save, skill gains, bed result, dawn, ordered voice playback, return of controls, removal of one glow dust, tracked quest, mine access, and a fresh resave. *Blood of the Daedra* stays at stage 15 while *Azura* reaches stage 20.

The final replay and stock reload both show **Afflicted Brethren** with a bearing and a distance of 212 m to the entrance. Facing the statue at 1280×720, the stock reload averaged 24.4 FPS over 489 frames with a 30 FPS cap. The 95th-percentile frame time was 59.2 ms. The screenshots are `build-windows/sponsor-level12b/NextObjective.png` and `build-windows/sponsor-level-stock03/Checkpoint.png`.

All 2,366 engine tests passed and matched the inventory in `build-windows/checks-e5133c77b8bb47fc9d47083ca753a71b`. The tested Windows executable has SHA-256 `b3293a3d346f6da1a99776ad9b5fbd2db4fbbb557dbe7ffd21922361bc01af0c`. The continuation save has SHA-256 `1925b89cfce50b8ad9005667dc0c2b9e2ac6abf353569ff41214473ac3047e2d`.

```powershell
$saves = 'build-windows/temple-player-ready/checkpoints'
$bruma = 'build-windows/sponsor-level06'
python scripts/verify_azura_offering.py "$saves/Blood-of-the-Daedra-Glow-Dust.omwsave" "$bruma/Trained.omwsave" build-windows/sponsor-level10/LevelTwo.omwsave "$bruma/Dawn.omwsave" "$saves/Azura-Offering-Accepted.omwsave" build-windows/sponsor-level-stock03/Reloaded.omwsave build-windows/sponsor-level12b/openmw.log
```
