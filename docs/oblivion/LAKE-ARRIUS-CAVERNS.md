# Lake Arrius Caverns and the doorkeeper

Run `build-windows/Continue-After-the-Doorkeeper.cmd`. The player is through the open wooden gate, facing Harrow as he walks up the passage. He asks the player to surrender their possessions. Both answers are available, and the player still has all four Commentaries. The next chunk starts with this choice at Dagon Shrine stage 30.

The checkpoint is `build-windows/temple-player-ready/checkpoints/Dagon-Shrine-Doorkeepers-Passage.omwsave`, SHA-256 `d1a6395f8e40a6aee6e1b52ac6e3e119587b67585fec8f22359bc47eab8eca07`. The [preceding checkpoint](GREEN-EMPEROR-WAY.md) starts at the Tomb of Prince Camarril.

## Gameplay and checks

The [Harrow escort checkpoint](HARROWS-ESCORT.md) continues from this passage through the handover and into the shrine.

From the tomb save, the player fast-travelled to Cheydinhal's west gate, exited to the stables, waited until daylight and ran to Lake Arrius, joining the eastern approach road. A camera and movement aid steered the player. The map, Wait menu, doors, dialogue choices and quicksaves used game inputs. Approaching the lake upgraded the cave from a visible marker to a discovered fast-travel destination and incremented Places Found once. Inside, stage 20 kept the Dagon Shrine objective while the player sought the doorkeeper. The replay and the dressed doorkeeper screenshot are in `build-windows/sponsor-arrius-final02`.

The final gate replay in `build-windows/sponsor-arrius-gate04` began at the stage 20 quicksave from that journey. The player answered “Dawn is breaking” with “Greet the new day.” The doorkeeper delivered all three voiced responses, walked to the door, invoked its original activation script and opened it. The player crossed the threshold and Harrow approached. `Passage.png` shows the checkpoint viewpoint. Two stock-control restarts ran the five-line greeting. `build-windows/sponsor-arrius-stock05/Harrow-choices.png` shows both answers. The resave in `build-windows/sponsor-arrius-stock06` retained the open animation at its endpoint, the robe and hood, the discovered marker and the player's Mythic Dawn rank. The final game SHA-256 is `060c043d1c839481cfbba3d68369b02accabd20df8036ef132175121b92e9836`.

At 1280 × 720 with a 30 FPS cap, the first measured 90 seconds of the overland run averaged 26.7 FPS, with a 45.6 ms 95th-percentile frame and 26 frames over 100 ms. The following 40 seconds averaged 29.2 FPS. Harrow's dialogue averaged 29.9 FPS across 25 seconds, with a 34.5 ms 95th-percentile frame. The samples use OSG reference times 100–190 and 190–230 in the journey profile, and 25–50 in the first stock restart. Outdoor streaming still has visible frame-time spikes. The city logs also retain a known butterfly mesh fallback and an unrelated NPC's unsupported GetGold command.

The full Windows inventories passed, covering 2,360 engine tests and 3,328 component tests. Source fingerprints, inventories and XML are in `build-windows/checks-39acc4362419450a8087f0eb8047141c`. All 212 portable Python tests passed.

```powershell
$start = 'build-windows/sponsor-arrius-final02'
$gate = 'build-windows/sponsor-arrius-gate04'
python scripts/verify_lake_arrius_passage.py "$gate/Passage.omwsave" --approach-save "$start/Before-password.omwsave" --journey-log "$start/openmw.log" --gate-log "$gate/openmw.log" --harrow-log build-windows/sponsor-arrius-stock05/openmw.log --restart-save build-windows/sponsor-arrius-stock06/Reloaded.omwsave
```

## Native behavior

The winning Oblivion master (SHA-256 `a26e21ea8c3041f8737ffb3a266129dedb7f8a88590625ecfecd5eb7f66b4a70`) supplies a reveal distance of 1,800 units, places the cave marker at `01e903`, and equips the doorkeeper with robe `024de2` and hood `08d755`. The hood uses its own head slot. The password INFO `01f10b` sets stage 30. Its quest result grants faction rank zero and assigns Find package `097cc4` targeting door `022865`. Door script `0cb551` checks the activator's faction or player identity before invoking the default door action.

In the original executable (SHA-256 `a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`), the marker update at `0x5A6FC6` uses three-dimensional distance. The reveal comparison is strict, with discovery updating the player statistic at `0x5A711B`. GetPCInFaction at `0x4F71C0` supplies the player to GetInFaction at `0x4F70C0`. Scripts, package conditions and dialogue now consult the same native faction membership.
