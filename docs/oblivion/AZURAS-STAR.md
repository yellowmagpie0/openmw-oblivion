# Azura’s Star

Run `build-windows/Continue-After-Azuras-Star.cmd` to resume outside Azura’s Shrine. The Knight carries Azura’s Star and faces the statue and the five candles. The next objective in *Blood of the Daedra* points to Martin at Cloud Ruler Temple.

From the [cleared mine](GUTTED-MINE.md), the Knight opened the wooden exit and followed the mountain road back on foot. Activating the statue in the afternoon played both of Azura’s recorded responses. The last response returned player control and granted one Star. Azura’s quest reached stage 100, *Blood of the Daedra* advanced to stage 20, and Fame rose from 7 to 8.

The first run exposed two presentation issues. Successful scripted item grants now show the item name and the localized acquisition message. The inventory portrait selects Oblivion’s idle for the equipped weapon, which lets the Knight hold the battle axe with both hands.

The full walk is in `build-windows/sponsor-azstar01/openmw.log`. The final reward replay starts at the altar save from that walk. Its HUD and inventory captures are in `build-windows/sponsor-azstar03`. The checkpoint SHA-256 is `c562397b87f2f190468ed6c2725d462985987ea273c1741d51fb5b2a9a699937`.

A fresh process with standard controls shows Martin at 1.44 km. At 1280×720 with a 30 FPS cap, the shrine view averaged 22.3 FPS across 448 frames with a 95th-percentile frame time of 74.7 ms. The screenshot, timing sample and resave are in `build-windows/sponsor-azstar-stock02`. The Windows executable SHA-256 is `310bf180ac125355e0e8db5e4bf56657606c7943ae9f1e2847761880dbf8a3e9`.

```powershell
$saves = 'build-windows/temple-player-ready/checkpoints'
python scripts/verify_azuras_star.py "$saves/Azura-Gutted-Mine-Cleared.omwsave" "$saves/Azuras-Star-Received.omwsave" build-windows/sponsor-azstar-stock02/Reloaded.omwsave build-windows/sponsor-azstar01/openmw.log build-windows/sponsor-azstar03/openmw.log
```
