# Reporting Jearl’s orders to Jauffre

From [Jearl’s basement](SPIES-JEARLS-ORDERS.md), take the trapdoor back into her house, leave Bruma through the north gate, and follow the mountain road up to Cloud Ruler Temple. In the overnight replay, Jauffre was in the East Wing, through the Great Hall. Choose **[Show Jauffre Jearl’s orders]** when he asks about the spies.

Jauffre acknowledges both spies’ deaths, promises to warn the Countess of Bruma, and sends the player to Martin. In Oblivion.esm, greeting `02006b` offers the choice and response `020085` plays three voiced lines and sets *Spies* to stage 90. The two journal entries record the warning and the lead to Martin. The stage script clears the quest-item flag on the scroll. The player keeps the document.

## East Wing replay

Jauffre and two Blades use a dining marker with a 500-unit radius. On the first visit both Blades stood within nine units of Jauffre. The overlap put Steffan under the crosshair when the player tried to speak to Jauffre. Resident and offscreen diners now recognize the authored area, and the game checks for a free position when bringing diners back into the scene. The correction also handles saves made when the actors were already overlapping. The dialogue portrait uses a softer fill light so Jauffre’s features remain visible in the candlelit room.

## Continue from the report

Run `build-windows/Continue-After-Reporting-to-Jauffre.cmd`. The checkpoint is `build-windows/temple-player-ready/checkpoints/Spies-Orders-Reported-to-Jauffre.omwsave`. The player is in the East Wing with stock controls. The compass points to Martin for the next conversation about the Mysterium Xarxes.

## Verification

The walk from the basement through Bruma’s north gate and up the switchbacks is recorded in `build-windows/sponsor-orders-full01`. The final replay starts at its `TempleGate.omwsave`, uses ordinary door activations and dialogue clicks, and ends after all three replies. Loading the Great Hall preserves Jauffre’s offscreen arrival in the East Wing, including when he stopped near the edge of the dining area.

The report ran at 29.7 FPS during a 456-frame dialogue sample at the profile’s 30 FPS limit, with a 95th-percentile frame time of 34.4 ms. Captures before, during and after the conversation are in `build-windows/sponsor-orders-room02`. The continuation save SHA-256 is `e12bf942fc094af06423c657dbf1455b462bfe345d24c4a1c01a78974ebfeb7a`.

```powershell
$saves = 'build-windows/temple-player-ready/checkpoints'
$run = 'build-windows/sponsor-orders-room02'
python scripts/verify_jauffres_spy_report.py "$saves/Spies-Jearls-Orders.omwsave" "$saves/Spies-Orders-Reported-to-Jauffre.omwsave" build-windows/sponsor-orders-stock01/Reloaded.omwsave --walk-log build-windows/sponsor-orders-full01/openmw.log --dialogue-log "$run/openmw.log"
```

The Windows game executable SHA-256 is `e1282e01d9fc1ba00251d50f1b8ca43e541cbcdc0bb96b3f4979269a9b9963cf`. The full Windows engine inventory passed all 2,365 tests. The portable Python gate passed all 212 tests. Logs are `build-windows/orders-room-tests.log` and `build-windows/orders-python-profile.log`. Broader Python discovery failed on this host, including the HUD capture tests that require ImageMagick’s `convert`.
