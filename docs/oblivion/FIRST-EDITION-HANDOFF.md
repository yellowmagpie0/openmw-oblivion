# First Edition and the Sponsor’s note

Run `build-windows/Continue-After-Gwinas.cmd` to resume just inside the bookstore facing the exit. The compass points through that door toward Baurus in the Elven Gardens District. Press Activate to leave. The owned lock remains intact and the tooltip shows **Exit permitted**. The [next checkpoint](ELVEN-GARDEN-SEWER-ESCORT.md) covers reporting the Sponsor’s meeting and following Baurus into the sewers.

## Played route

The [Tar-Meena checkpoint](ARCANE-UNIVERSITY-TAR-MEENA-HANDOFF.md) starts in the Arch-Mage’s Lobby. Leave through the front door, select the Market District from the world map and walk to First Edition. Speak to Phintias across the counter about the Mysterium Xarxes, ask for Volumes 3 and 4, then ask about Gwinas. Wait on the shop floor. Gwinas walks in, stops opposite Phintias, asks for his order and thanks him after his reply.

Speak to Gwinas about Volume 3 and the Mythic Dawn. Explain that the cult murdered the Emperor, ask for his book, then ask about the fourth volume. His last response gives the player the Sponsor’s note. The handoff is at MQ05 stage 72 with one copy each of Volumes 1–3 and one note. Baurus has reached the Elven Gardens rendezvous. The sewer meeting is still ahead.

## Verification

The installed `Oblivion.esm` has SHA-256 `a26e21ea8c3041f8737ffb3a266129dedb7f8a88590625ecfecd5eb7f66b4a70`. Stage 50 has a textless result that enables Gwinas and starts him toward the shop. His Find package targets Phintias. The three voiced responses are `022b39`, `022b51` and `022b53`. Gwinas’s package completion moves one Volume 3 from the chest to his inventory. The peaceful handoff selects stage 45 entry 4. Stage 70 gives the note and directs the player back to Baurus.

```powershell
python scripts/verify_first_edition.py build-windows/temple-player-ready/checkpoints/Path-of-Dawn-Return-to-Baurus.omwsave --route-log build-windows/bookshop-release06/openmw.log
python scripts/verify_first_edition.py build-windows/bookshop-cold11/Restarted-handoff.omwsave
.\scripts\windows-dev.ps1 -Action Test -TestBinaries openmw-tests,components-tests -Jobs 5
```

The first command checks the full played route and final inventories. The second checks a new save made after a stock-control restart. The third runs the engine and component tests and compares their results to each executable’s test inventory. A separate restart from the middle of Gwinas’s first line is in `build-windows/bookshop-resume07`. Phintias and Gwinas finished the two remaining responses, the completion event fired once, and the chest was empty with one book on Gwinas. The handoff save SHA-256 is `ec388f676d055b4e281e5b6d532b6fc36cc5df6b4b262ebcc0bc178abdd448e0`.

The counter scene is visible in `build-windows/bookshop-release06/At-the-counter.png`. An earlier Market District run crashed during a vertex-buffer upload. FaceGen now gives each actor’s morphed head vertices their own buffer. A renderer regression checks that creating a head leaves the template’s shared buffer alone. The full route and subsequent restarts completed after the fix.

The final executable replayed the entire route from Tar-Meena’s lobby save. Run the same verifier on `build-windows/bookshop-delivery12/After-Gwinas.omwsave` with its `openmw.log`. All 2,351 engine tests and 3,325 component tests passed against the enumerated inventories in `build-windows/checks-e167baec1a054ce5b29f7245a96e19c3`.

## Frame timing

The Market District save contains 105,768 references. In a main-thread sample, 192 of 550 samples fell inside construction of the saved-reference index. Appending a few records caused the next AI, scripting and guidance passes to sort the entire index again. The lookup now searches the sorted prefix in place and indexes only the remaining records.

In matched 1280 × 720 stock-control views outside First Edition, the first 30-second sample after a 35-second warmup improved from 12.6 to 16.5 FPS. The 95th-percentile frame fell from 102 to 86 ms. A second 30-second window starting at 85 seconds improved from 11.2 to 14.3 FPS. The original samples are in `build-windows/bookshop-market09/stats.txt` and the revised samples in `build-windows/bookshop-market10/stats.txt`. Market District mechanics and initial cell loading still cause visible hitches.

Looking at the shop exit from the delivered save, the same 35–65 second window improved from 17.1 to 27.1 FPS, with 95th-percentile frame time down from 78 to 47 ms (`build-windows/bookshop-cold08/stats.txt` and `build-windows/bookshop-cold11/stats.txt`). The 30 FPS cap was active in both runs. The final game executable used for the restart has SHA-256 `e581192d0044e2d4c31980ce9610c09003a9cd7d3d4a9a28d6dcc947c00eba87`.

During the player’s walk across the Market District, the 45-second window starting 50 seconds into gameplay averaged 11.1 FPS in `bookshop-release06` and 14.6 FPS in `bookshop-delivery12`. The 95th-percentile frame fell from 146 to 97 ms.
