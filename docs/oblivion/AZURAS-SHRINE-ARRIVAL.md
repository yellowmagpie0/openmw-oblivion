# The journey to Azura’s Shrine

From [Martin’s briefing](BLOOD-OF-THE-DAEDRA-BRIEFING.md), leave Cloud Ruler Temple through the Great Hall doors. Use the world map to travel to the previously discovered Lake Arrius Caverns. Follow the roads east and north into the mountains, then take the trail south to the shrine. The journey passes Wind Range Camp. Approaching Azura’s Shrine changes its map marker from revealed to discovered and enables return travel.

Mels Maryon introduces the **Shrine of Azura** topic. On this save he initially asks the player to persuade him. Cast the Imperial power **Voice of the Emperor** on him and reopen the conversation. Choose **I wish to summon Azura** to hear his advice about bringing glow dust at dawn or dusk. Both lines play. His response in the installed master has no quest-stage result. The level-one character can activate the statue and hear the advice in the same visit. In the afternoon the statue displays the message that Azura requires a more powerful champion.

## Continue playing

Run `build-windows/Continue-At-Azuras-Shrine.cmd` to load `Blood-of-the-Daedra-At-Azuras-Shrine.omwsave` with stock controls. The player faces the statue. *Blood of the Daedra* remains at stage 15. The next prerequisite for this shrine is level 2. Its offering is glow dust at 5–7 a.m. or 5–7 p.m.

## Verification

The temple departure and mountain road are recorded in `build-windows/sponsor-azura01`. On the original approach, the nearby Ayleid well issued 158 pairs of animation requests between the last trail waypoint and the first greeting. The well mesh specifies a one-frame opening and a 6.67-second cyclic idle. The original executable’s ungrouped object animation query scans the controller sequences for active playback (1.2.0416, command `0x1128`, handler `0x510a90`). The local disassembly is in `build-windows/azura-anim-oracle.txt`.

The final replay in `build-windows/sponsor-azura03` starts at the uphill checkpoint from that journey. The well started its opening and idle once each through discovery, conversation and altar activation. The saved idle continued after a fresh launch with stock controls. The 20-second stock sample facing the statue averaged 23.3 FPS across 469 frames with a 63.3 ms 95th-percentile frame time and a 30 FPS cap.

The Windows executable SHA-256 is `a36bd64e2807c4921b265bb16aefb303322f3ebb80f071c303a412792c076140`. The continuation save SHA-256 is `07e3bae5fd07445553f4e94191cd692b4ccd33b32c78a64b897229611181f6f0`. All 2,366 engine tests passed and matched the inventory in `build-windows/checks-fd6ee565c7e948ebbded8a8e7d5aa0dc`.

```powershell
$saves = 'build-windows/temple-player-ready/checkpoints'
$run = 'build-windows/sponsor-azura03'
python scripts/verify_azuras_shrine_arrival.py "$saves/Blood-of-the-Daedra-Azuras-Shrine-Lead.omwsave" "$run/ShrineArrival.omwsave" "$saves/Blood-of-the-Daedra-At-Azuras-Shrine.omwsave" build-windows/sponsor-azura-stock01/Reloaded.omwsave build-windows/sponsor-azura01/openmw.log "$run/openmw.log"
```
