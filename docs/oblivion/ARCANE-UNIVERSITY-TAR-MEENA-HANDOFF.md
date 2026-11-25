# Tar-Meena’s briefing at the Arcane University

Start with `build-windows/Continue-After-Baurus-Basement.cmd`. Leave the basement through the boarding house and go outside. Fast-travel to the Arcane University from the world map. Walk up the front steps, enter the Arch-Mage’s Lobby and speak to Tar-Meena. Choose **Mythic Dawn**, tell her you found a book, explain that you need to find the cult and ask where to find the other books.

The course ends with *The Path of Dawn* at stage 40. Both stage-40 journal entries are recorded. The player carries one copy each of Volumes 1 and 2. The stock stage script places one copy of Volume 3 in the First Edition chest in the Market District. The compass changes to **First Edition**. Continue from the lobby with `build-windows/Continue-After-Tar-Meena.cmd`. The [First Edition handoff](FIRST-EDITION-HANDOFF.md) follows the lead to Phintias and Gwinas.

## Replaying the checks

The starting save and its provenance are in the [basement handoff](LUTHER-BROADS-BASEMENT-HANDOFF.md). The isolated route in `build-windows/university-route01` follows the player through both boarding-house doors, world-map travel and the tower entrance. Its `Before-Tar-Meena.omwsave` reproduces the approach from behind her bench. Use the same position to compare the greeting before and after the camera change. The presentation keeps the speaker in her chair, checks clearance in front of her and restores the player’s view when dialogue ends.

In the installed Oblivion.esm (SHA-256 `a26e21ea8c3041f8737ffb3a266129dedb7f8a88590625ecfecd5eb7f66b4a70`), the final response is INFO `022b43`. Its three voice files are numbered 2 through 4. The stage result grants Volume 2 to the player and Volume 3 to chest `022b09` in a different cell. The focused check reads those two inventories independently.

```powershell
python scripts/verify_tar_meena_briefing.py build-windows/temple-player-ready/checkpoints/Path-of-Dawn-First-Edition.omwsave --route-log build-windows/university-integrated10/openmw.log --dialogue-log build-windows/university-integrated10/openmw.log
```

For a stock-control restart, load the checkpoint, quicksave again and run the same script on that new save. The log arguments are optional. Inspect the greeting and handoff captures separately for face visibility and the compass label.

## Frame timing

The Argonian head mesh has white specular colour and zero gloss. An isolated material comparison reproduced the orange wash under the lobby candles. The renderer suppresses that highlight on zero-gloss Oblivion materials and adds fill light to interior dialogue portraits. The voiced briefing averaged 29.9 FPS during a 60-second sample at 1280 × 720 with a 30 FPS cap.

After the conversation, the compass was calculating routes for three other quests before selecting *The Path of Dawn*. It now evaluates the tracked quest first, then considers other candidates in priority and form-key order. In matched stock-control runs from the same lobby save, average frame rate rose from 17.4 to 21.5 FPS. The 95th-percentile GUI update fell from 65.4 to 11.2 ms. Overall 95th-percentile frame time fell from 120.8 to 69.9 ms. Unpaused mechanics updates still produce hitches. The 30-second samples begin 35 seconds into `build-windows/university-perf07/stats.txt` and `build-windows/university-perf09/stats.txt`.
