# Gutted Mine

Run `build-windows/Continue-After-Gutted-Mine.cmd` to resume at the mine exit with 113 health. All five Afflicted Brethren are dead. The tracked objective points through the wooden door toward Azura’s Shrine. Return to the statue to collect the reward.

From the [shrine offering checkpoint](AZURAS-OFFERING.md), follow the mountain road to the unlocked mine entrance. A tripwire and swinging mace guard the approach to the first Afflicted Brethren. Two more occupy the front workings. The pull rope beside the rock wall opens the passage to the remaining pair.

## Corpse equipment

Taking the first Brethren’s equipped iron longsword used to close the game. Rebuilding his equipment replaced a shield node that the renderer was tracking as part of the ragdoll skeleton. Rigid attachments now identify their model root as equipment when they attach to a bone. The ragdoll can keep its skeleton while the inventory changes.

The failing pickup is recorded in `build-windows/sponsor-azmine04/openmw.log`. On the rebuilt engine, the player looted the same body, equipped the sword, fought the next two Brethren, rested, pulled the rope and walked through the opening. The pickup and wall screenshots are in `build-windows/sponsor-azmine05`.

The Orc in the lower workings equips an iron claymore. His shield previously stayed on his left forearm while both hands swung the blade. NPC and player outfit assembly now use the equipped weapon’s handedness to decide whether to attach the shield model. The shield stays in its inventory slot. Bethesda describes this equipment rule in the inventory section of the [Oblivion PC manual](https://assets.ctfassets.net/rporu91m20dc/35hpuLKJ2wsgsaWgKmsqaW/1ba5c2c99d147348094c06d0c8649b79/manual_oblivion-goty_pc_en-us.pdf).

## Encounter and continuation

The road approach starts from the accepted offering in `build-windows/sponsor-azmine03`. After opening the rock wall, the Knight took an iron battle axe from a fallen Brethren and carried it into the lower workings. An isolated replay at difficulty −20 captured the corrected two-handed stance in `build-windows/sponsor-azmine16/Claymore.png`.

The continuation comes from a separate run at the original difficulty. The Knight wounded the Orc, retreated through the mine and used Heal Minor Wounds on the road. Both remaining Brethren pursued him to the entrance passage. He re-entered the mine, killed them there and rested for one hour. The Orc still carries the Worn, Faded Note. The fight and recovery are recorded in `build-windows/sponsor-azmine18/openmw.log`.

Azura’s own death counter advanced to five. Her quest reached stage 30, enabled the five memorial lamps and their light, and added the return objective. *Blood of the Daedra* remains at stage 15. The checkpoint SHA-256 is `7f56592df7bb9572cec25adb7a532607ba5616738703c36df42b17f9d7a4fd38`.

A fresh process with standard controls shows **Azura’s Shrine** and a distance of 3 m to the exit door. At 1280×720 with the 30 FPS cap, the exit view averaged 29.0 FPS over 581 frames with a 95th-percentile frame time of 41.9 ms. The screenshot, timing sample and resave are in `build-windows/sponsor-azmine-stock01`.

All 2,366 engine tests passed with an exact inventory match in `build-windows/checks-c3aa08f71535480292c8823f735df274`. The Windows executable SHA-256 is `d4d22ceef40da50bc05b441202f40753f8e50718065f8be4efea794fe3ba448b`.

```powershell
$saves = 'build-windows/temple-player-ready/checkpoints'
python scripts/verify_gutted_mine.py "$saves/Azura-Offering-Accepted.omwsave" "$saves/Azura-Gutted-Mine-Cleared.omwsave" build-windows/sponsor-azmine-stock01/Reloaded.omwsave
```
