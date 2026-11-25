# Baurus and the Elven Garden sewer entrance

Run `build-windows/Follow-Baurus-in-the-Sewers.cmd` to resume at the foot of the entrance stairs. Baurus is visible seven metres ahead, the player has a torch equipped, and the compass points to him. Walk toward him to continue to the wheel. The next gameplay check is the wheel and the approach to the Lower Hall.

The route starts at the [First Edition checkpoint](FIRST-EDITION-HANDOFF.md). Leave the shop, walk through the Market District gate into the Elven Gardens, and speak to Baurus about the **Mythic Dawn meeting**. The conversation advances *The Path of Dawn* to stage 75. Baurus then leads the player through the streets and goes down the grate ahead of them.

The installed `Oblivion.esm` has SHA-256 `a26e21ea8c3041f8737ffb3a266129dedb7f8a88590625ecfecd5eb7f66b4a70`. The briefing is INFO `022cf0`. The first escort package `09589d` targets the player with a follow distance of 500 units and leads toward the sewer wheel. The district entrance `022cea` exits at `023d2e` in the Elven Garden Sewers. A later package takes Baurus to the Lower Hall, where its completion advances the quest to stage 76.

## Runtime checks

An early street replay showed Baurus walking into pedestrians on a route that doubled back. The revised exterior route uses the loaded navigation mesh to approach the selected grate across district grid lines. A separate replay at Luther Broad’s Boarding House showed Baurus remaining in his greeting stance after the meeting topic advanced the quest. He now reevaluates that assignment and leaves through the front door. The preserved reproduction and restart are in `build-windows/sponsor-course03` and `build-windows/sponsor-inn04`.

During a delayed player crossing in `build-windows/sponsor-delivery05`, Baurus advanced thousands of units out of view and reached the wheel before the player caught up. The escort now observes its follow distance across exterior grid lines and waits on the far side of a load door when the player is still in the district.

Two saves taken twenty seconds apart outside the grate recorded Baurus at the same position inside. On entering, the player stood still at the top of the stairs. Baurus stepped around the player and descended. The player followed him into the passage and equipped a carried torch. The street and passage views are in `build-windows/sponsor-delivery05/Baurus-leading.png` and `build-windows/sponsor-landing07/Baurus-in-the-sewers.png`.

The delivered save retains the first three Commentaries and the Sponsor’s note. A fresh process loaded it with the stock player script. Baurus waited while the player stood still, then advanced when the player walked forward with the keyboard. The restart used game executable SHA-256 `9d6a06a8cca7284c7c6ba701cd061cd897bd8b128ebb44391f2e1e02e9b37cb`.

A second run on this executable played from the bookstore through the briefing, grate and entrance stairs in a single session. The complete engine and component inventories passed with 2,353 and 3,325 tests respectively. The inventory lists, results and source fingerprint are in `build-windows/checks-0a4e1ba963a74631bdba39a332848d65`.

```powershell
python scripts/verify_baurus_sewer_escort.py build-windows/temple-player-ready/checkpoints/Path-of-Dawn-Elven-Garden-Sewers.omwsave --route-log build-windows/sponsor-final06/openmw.log --outside build-windows/sponsor-final06/Waiting-at-the-grate.omwsave build-windows/sponsor-final06/Still-at-the-grate.omwsave
python scripts/verify_baurus_sewer_escort.py build-windows/sponsor-cold08/Restarted-escort.omwsave --after-walking build-windows/sponsor-cold08/Resumed-walk.omwsave
python scripts/verify_baurus_sewer_escort.py build-windows/sponsor-full09/Bookstore-to-sewers.omwsave --route-log build-windows/sponsor-full09/openmw.log
```

## Frame timing

At 1280 × 720 with the 30 FPS cap, a 25-second street sample starting 85 seconds into `sponsor-delivery05` averaged 18.2 FPS with a 73.5 ms 95th-percentile frame. The torchlit sewer view averaged 19.1 FPS and 70.8 ms over seconds 15–35 of the stock restart in `sponsor-cold08`. Mechanics took 38.7 ms per frame on average in the sewer sample. The route still has visible frame drops.
