# Burd and Jearl’s orders

From the [runestone checkpoint](SPIES-RUNESTONE-ENCOUNTER.md), walk to Bruma’s north gate and enter the castle. In the evening replay, Captain Burd was in the barracks off the Great Hall. Ask about **Spies** and tell him Jearl was a Mythic Dawn agent. Leave the castle, enter Jearl’s house and open the trapdoor with the basement key. The orders lie on a table beside a book. Read them and take them for Jauffre.

## Quest and runtime evidence

In the same Oblivion.esm identified in the runestone handoff, Burd’s response `02008d` sets MQ07 to 50. The dead-agent reply `038bc6` grants search authorization and sets stage 70. Activating the placed scroll `06719d` runs its own script `00c02d`, which opens the reader and sets stage 80. Taking the orders puts base item `00c026` in the player’s inventory. The next objective is to show them to Jauffre at Cloud Ruler Temple.

The initial barracks run hit repeated frame errors during ambient dialogue selection. The corrected conditions read NPC sex, player fame and disease state from their gameplay owners. An unresolved condition skips the affected response and logs a diagnostic so unrelated dialogue and simulation can continue. The barracks sleep shift uses a zero-radius room marker. Guards now select individual beds in the room and can use the assigned bed from its edge.

The first basement visit exposed a script failure when the saved reference cache had no entry for the newly visited scroll. The interaction now captures the placed reference directly. Scrolls open in the scrolling reader, and Take uses the same native pickup path as books.

## Continuation

Run `build-windows/Continue-After-Finding-Jearls-Orders.cmd`. The checkpoint is `build-windows/temple-player-ready/checkpoints/Spies-Jearls-Orders.omwsave`. It leaves the player in the searched basement with the document.

The approach from the runestone uses assisted walking and ordinary door activations. The final dialogue replay resumes that walk in the barracks from `build-windows/sponsor-burd01/BeforeBurd.omwsave`. Burd’s permission is captured in `build-windows/sponsor-burd-final02/SearchAuthorized.png`. The basement replay in `build-windows/sponsor-burd-basement-final` starts from the subsequent barracks save. Its `ReadOrders.png` and `OrdersSignature.png` captures show both ends of the scroll. The continuation uses stock controls. Its save SHA-256 is `33343a33ac6edeb4ac1e21b7be696823077c082482cfa13d9d271c316ec28c4d`.

```powershell
$saves = 'build-windows/temple-player-ready/checkpoints'
$run = 'build-windows/sponsor-burd-final02'
python scripts/verify_brumas_spy_orders.py "$saves/Spies-Runestone-Encounter.omwsave" "$run/SearchAuthorized.omwsave" "$saves/Spies-Jearls-Orders.omwsave" build-windows/sponsor-burd-stock02/Reloaded.omwsave
```

The final stock barracks probe in `build-windows/sponsor-burd-sleep03` observed all four guards on package `0362aa` settle into separate beds. The last arrived about two real minutes after loading. A second officer can still stand close enough to clip Burd’s shoulder when the player lingers after the conversation.

The final Windows game executable SHA-256 is `9efc9a403db7f8a182df6b5dd84ba5b5e076c33216753b18a833dd99cca93e82`. Its 2,364 engine tests passed (`build-windows/burd-bed-arrival-tests.log`). The integration run also passed 3,332 component tests and 212 Python tests (`build-windows/burd-tests.log` and `build-windows/burd-python.log`).
