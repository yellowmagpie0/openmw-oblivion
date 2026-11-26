# Jauffre and Steffan's spy briefings

Run `build-windows/Continue-After-Steffans-Briefing.cmd` to resume after speaking to Captain Steffan. The next lead is the runestone on the road below Cloud Ruler Temple at dusk.

From [Martin’s handoff](MARTINS-XARXES-HANDOFF.md), leave the east wing and speak to Jauffre in the courtyard. Ask about **Spies** and agree to find them. In the recorded run Steffan was in the east wing. Walk back inside, speak to him and select **Spies**. The captains’ locations depend on their schedules.

## Content and runtime checks

In the original master, Jauffre's response `020075` offers the choice. Accepting plays `020076` and sets MQ07 to stage 20. The stage enables Jearl and offers both Steffan and Burd as targets. Steffan's response `020077` sets stage 25 and gives the runestone lead. The quest now targets the observation point `02007b`.

The first run exposed two presentation issues. The single compass arrow selected Burd by record order even at the temple. The HUD now considers all active approaches, preferring a reachable target in the player's current interior and otherwise the nearest local marker. The world map retains both leads. The star field also ended in a straight line behind Jauffre. Its opacity now tapers toward the horizon in the Oblivion sky shader.

The replay starts at stage 10 and uses assisted walking and camera steering, ordinary door activation and dialogue clicks. Runtime captures and the intermediate save are under `build-windows/sponsor-spies-brief02`. The continuation uses stock player controls. To check both journal entries, Jearl’s activation, dialogue playback and fresh-process persistence, run:

```powershell
$saves = 'build-windows/temple-player-ready/checkpoints'
$run = 'build-windows/sponsor-spies-brief02'
python scripts/verify_spies_briefing.py "$saves/Mysterium-Xarxes-Delivered.omwsave" "$run/JauffreOrders.omwsave" "$saves/Spies-Steffan-Briefing.omwsave" build-windows/sponsor-spies-stock01/Reloaded.omwsave --log "$run/openmw.log"
```

In the rebuilt replay, the compass showed Captain Steffan at 24 m in the courtyard and 2 m beside him. The timed dialogue samples averaged 29.4 and 30.0 FPS at the profile's 30 FPS limit. The 108-second sample including movement, cell loads and saves averaged 25.7 FPS with a 46.9 ms 95th-percentile frame time. The checkpoint SHA-256 is `9a8862bb748a97453098ea80e587e37787fc3a936347235f19a2a2bf7fb79e18`.
