# Baurus’s warning at Luther Broad’s

Start with [Jauffre’s briefing](PATH-OF-DAWN-BRIEFING-HANDOFF.md). Fast travel to the Imperial City Elven Gardens District, walk through the district to Luther Broad’s Boarding House and enter through the front door. Baurus sits at the bar. Astav Wirich watches from a chair across the room. Activate the empty stool beside Baurus. His third reply, “I’m ready when you are”, advances The Path of Dawn to stage 25. The journal asks the player to follow the man watching them.

## Authored scene

In the original Oblivion.esm (SHA-256 `a26e21ea8c3041f8737ffb3a266129dedb7f8a88590625ecfecd5eb7f66b4a70`), Baurus’s UseItemAt package `01f10c` specifies stool `0549df` and beer `0b1200`. Astav’s package `022b0b` specifies chair `022b0a` and the Book object type. The adjacent stool `0549e1` uses a forward entry clip rotated a quarter turn. MQ05Script starts Baurus’s greeting when the player is within 200 units and fully seated. Greeting `022b13` supplies three choices. Response `022b16` sets stage 25 and reevaluates Baurus’s package.

The engine now uses the furniture marker’s direction for the entry clip and its rotation for placement. NPCs using an explicitly assigned chair reserve its marker, play the chair pose, and release it when their package changes. An NPC can use a book without consuming it. A repeating conversation trigger preserves the voice line and replies already on screen. Seated dialogue directs the player’s view toward the speaker.

## Verification

Run `python scripts/verify_luther_broads_surveillance.py SAVE --log OPENMW_LOG` for the complete course. Omit the log to check the journal, actors and standing player in a separately reloaded save. The full route uses the world map, player movement, door and stool activation, dialogue choice and quicksave.

The final replay in `build-windows/surveillance-course06` loaded the briefing save, travelled 10.3024 in-game hours and walked from the district marker to the inn. Both actors occupied their assigned seats before the player sat. Both of Baurus’s responses resolved to voiced lines with lip animation. After the player agreed, Baurus stood first and entered the basement. The player stood up, Astav started following Baurus and the HUD switched to Astav. Stage 30 had fired by the time the player saved. The stage 25 journal entry remained active and Astav still carried his copy of *Mythic Dawn Commentaries 1*.

`build-windows/Meet-Baurus-at-Luther-Broads.cmd` opens the arrival save. `build-windows/Continue-After-Baurus-Warning.cmd` opens the standing checkpoint with the watcher heading for the basement. Their SHA-256 digests are `97f5623925cae2d072335afccab53e69e15b9d89acef53baea33f4c114269014` and `1b53e705d504a72d2253a74fd28ca02e661b1a77c3a7e3d8f33619e52fa28075`, respectively. A stock-control reload reproduced the journal, follow package and standing player. A subsequent stock quicksave placed both NPCs in basement cell `047ca3` while the player remained upstairs.

At 1280 × 720 with a 30 FPS cap, the final 50 seconds inside the inn averaged 28.2 FPS over 1,408 frames with a 38.1 ms 95th-percentile frame time. The full route averaged 24.1 FPS including the map and area transitions. A separate cold reload of the departure save averaged 18.8 FPS in its first post-startup sample and 26.3 FPS over its final 30 seconds. The longest cold frames spent most of their time in mechanics updates.

The game executable SHA-256 is `1cd5cd449b59f1b1b54fd28b73ffaf6fed8891eafa71796fa639db2e140b9d41`. Regression coverage includes repeated scripted greetings, explicit dialogue topics, arrival at a raised chair and the saved quest book. The full engine and component inventories cover 2,348 and 3,324 tests. The Python runtime-state suite covers 205. The integration log at `build-windows/surveillance-integration.log` identifies the inventory, XML and source-fingerprint directory.

The next playable objective is to follow Astav through the basement door and witness his encounter with Baurus.
