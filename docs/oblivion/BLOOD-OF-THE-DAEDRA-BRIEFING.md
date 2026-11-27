# Martin’s first ritual ingredient

After [reporting Jearl’s orders to Jauffre](SPIES-REPORT-TO-JAUFFRE.md), leave the East Wing and speak to Martin at his desk in the Great Hall. Hear his explanation of the four ingredients, then choose **Daedric artifact**. The compass points to the temple’s copy of *Modern Heretics*. Aim at its exposed spine in the stack on the desk, open it and close the reader. The book stays on the desk. Azura’s Shrine appears on the map and the compass leads to the Great Hall doors.

## Continue playing

Run `build-windows/Continue-After-Reading-Modern-Heretics.cmd` to load `Blood-of-the-Daedra-Azuras-Shrine-Lead.omwsave` with stock controls. The next leg is [the journey to Azura’s Shrine](AZURAS-SHRINE-ARRIVAL.md).

## Runtime checks

The fresh replay in `build-windows/sponsor-daedra05` starts in the East Wing. It exercises the connecting door, Martin’s existing interrupted study package, the conversation, the book reader and the shrine reveal. The game recovers Martin’s seat at the desk and frames his face above the response pane. The captures `Ritual.png`, `BookObjective.png`, `LibraryCopy.png` and `ShrineObjective.png` show the sequence. During 619 frames of the opening response, the game averaged 28.3 FPS at the profile’s 30 FPS cap, with a 95th-percentile frame time of 34.4 ms.

The winning master places the targeted book in `CloudRulerTempleGreatHall`. Its quest condition requests the `CloudRulerTemple` cell group. The original executable’s `GetInCellParam` evaluator checks the explicit reference’s parent cell and compares editor IDs using the requested prefix, ignoring ASCII case. The local disassembly is in `build-windows/daedra-cell-oracle.txt`. The runtime sequence earns *Blood of the Daedra* journal entries 10, 15 and 9 in that order, and completes *Spies*. The saved current stage stays at 15. The shrine is revealed and has yet to be visited.

The replayed Windows executable has SHA-256 `00a5b19f38c0d65b02d9f7d045bc4977f16ef113afd1990f942e5ec839bdd9c3`. The continuation save has SHA-256 `295366fea9026088acf0122ecb8dbad2a1432ab92cea7d8a81ded0cbf6429ebe`. All 2,366 engine tests passed in `build-windows/daedra-final-tests.log`.

```powershell
$saves = 'build-windows/temple-player-ready/checkpoints'
$run = 'build-windows/sponsor-daedra05'
python scripts/verify_martins_daedric_artifact_lead.py "$saves/Spies-Orders-Reported-to-Jauffre.omwsave" "$run/BookLead.omwsave" "$saves/Blood-of-the-Daedra-Azuras-Shrine-Lead.omwsave" build-windows/sponsor-daedra-stock01/Reloaded.omwsave "$run/openmw.log"
```
