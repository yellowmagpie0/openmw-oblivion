# The spies at the Hestra Runestone

Start at Steffan’s briefing in the east wing of Cloud Ruler Temple. Walk out through the courtyard gates, follow the switchbacks down the mountain and leave the road for the Hestra Runestone. Wait for dusk. Jearl takes the southern lookout and Saveri approaches the northern lookout from Bruma. They can arrive separately. Jearl carries the keys to her house and basement.

## Records and observed failures

The winning Oblivion.esm (SHA-256 `a26e21ea8c3041f8737ffb3a266129dedb7f8a88590625ecfecd5eb7f66b4a70`) gives Jearl package `02007d` at 16:00 for eight hours and Saveri package `02007e` at 18:00 for ten hours. The destinations are `02007b` and `02007a`. Their death scripts set MQ07 stages 30 and 31 and each increments the quest’s dead-agent count. Taking the basement key runs its OnAdd script and unlocks the next investigation lead.

Both agents originally remained on morning sleep packages through the evening. The AI now checks expired schedules during rest and travel, accounts for instantaneous package transitions when advancing the clock, and moves an approaching low-process actor back into the physical scene when its simulated position reaches the player’s vicinity.

The stone’s offer originally closed without equipping the player. Its scripted self-cast now grants the bound weapon and gauntlets, tracks the remaining duration across saves, and restores the displaced equipment on expiry.

## Replay and continuation

The descent from `Spies-Steffan-Briefing.omwsave` and the pre-wait save are in `build-windows/sponsor-runestone02`. The rebuilt encounter in `build-windows/sponsor-runestone07` resumes that walk at the stone. The pilot steers the player and uses regular wait, activation, attack and loot controls. Both NPCs follow their own packages. The continuation launcher uses stock player controls.

After the 15-hour wait, Jearl attacked shortly after 18:15. Saveri arrived from Bruma around 20:00. The player survived with 42 health, both keys and a zero bounty. The final walk steps clear of the stone and sheathes the dagger. Over a 250-second sample covering the encounters, the traced run averaged 26.5 FPS at a 30 FPS limit, with a 50.5 ms 95th-percentile frame time.

Run `build-windows/Continue-After-the-Runestone-Encounter.cmd` to continue the investigation in Bruma. The save check reads the independently reloaded and resaved checkpoint alongside the original combat saves.

The next [Bruma investigation](SPIES-JEARLS-ORDERS.md) follows the keys to Captain Burd and Jearl’s basement.

```powershell
$run = 'build-windows/sponsor-runestone07'
$stock = 'build-windows/sponsor-runestone-stock02'
$save = 'build-windows/temple-player-ready/checkpoints/Spies-Runestone-Encounter.omwsave'
python scripts/verify_runestone_encounter.py "$run/RuneArmed.omwsave" $save "$stock/Reloaded.omwsave" --expired "$stock/Expired.omwsave"
```

The checkpoint SHA-256 is `0fb6ad86ae72ac1e423151a9c6a7db6c2901b0c676012aa7fd19471e0b6dbc06`. The Windows game binary SHA-256 is `de9aa44555e71daaafb60556a39b2f66d44dab47d6fa4593cdf77add62b16775`. The Windows integration run passed 2,363 engine tests, 3,332 component tests and 212 Python tests. Logs are in `build-windows/runestone-native-tests.log` and `build-windows/runestone-python-final.log`.
