# Harrow's escort into Dagon Shrine

Run `build-windows/Continue-Inside-Dagon-Shrine.cmd`. The player is wearing the initiate robe, seven metres behind Harrow in the shrine entrance. Walk toward him to start Mankar Camoran's sermon. The checkpoint is `build-windows/temple-player-ready/checkpoints/Dagon-Shrine-Initiates-Arrival.omwsave`, SHA-256 `869f9d71687908814d1d3c00e1e62ea50d0390bf1043846a7d32d0fa99548c16`.

## Handover and escort

The course began at the [doorkeeper checkpoint](LAKE-ARRIUS-CAVERNS.md). Choosing to give Harrow all possessions used to advance the journal while leaving the original equipment on the player and the shrine door locked. Harrow could remain in his greeting package after dialogue closed.

The replay selected the surrender response, equipped the robe through the quest script, followed Harrow through the antechamber and entered the shrine using both doors. A steering aid supplied player movement and camera input. Dialogue, door activations and quicksaves used game inputs. The saved quest is at stage 40, before the sermon trigger. Harrow carries the surrendered gear, including gold and the scripted weapon. The player retains Commentaries volume III, the Sigil Keep Key and the five non-playable tutorial Blades armor pieces already in the source save.

The winning Oblivion master gives the surrender response `01f168` and stage 40 the inventory transfer, robe and shrine-door unlock. Harrow's escort package `01f87d` names the player as the escort target. The original executable's RemoveAllItems handler at `0x5074F0` calls the inventory transfer at `0x492E70`. Both its base and instance paths check biped playability. The player branch checks quest-item status. Executable and master hashes are in the [preceding handoff](LAKE-ARRIUS-CAVERNS.md#native-behavior).

## Evidence

`build-windows/sponsor-harrow05` contains the before-and-after dialogue and escort log, the initiation and arrival saves, and three gameplay screenshots. The final escort sample at 1280 × 720 with a 30 FPS cap averaged 27.2 FPS over 60 seconds, with a 45.6 ms 95th-percentile frame and six frames over 100 ms. The sample spans both cell transitions, OSG reference times 34–94. The 32 seconds inside the antechamber averaged 28.4 FPS with no frames over 100 ms.

```powershell
$checkpoints = 'build-windows/temple-player-ready/checkpoints'
$run = 'build-windows/sponsor-harrow05'
python scripts/verify_harrow_escort.py "$checkpoints/Dagon-Shrine-Doorkeepers-Passage.omwsave" "$checkpoints/Dagon-Shrine-Initiates-Arrival.omwsave" --initiate "$run/Initiate.omwsave" --restart build-windows/sponsor-harrow-stock07/Reloaded.omwsave --log "$run/openmw.log"
```

On a fresh launch with stock controls, the save retained the gear transfer, robe, door unlock and both characters' positions. See `build-windows/sponsor-harrow-stock07/Checkpoint.png`. A separate run walked forward to start the voiced sermon. The compass kept Dagon Shrine on screen when Harrow's marker disappeared, as shown in `build-windows/sponsor-harrow-scene06/Greeting.png`. Final game SHA-256 is `1ace5d9c0a0725e69ab0441545aa6abc01d12f69cfc43f968aede84e98866079`. All 2,361 engine, 3,328 component and 212 portable Python tests passed. The Windows inventories and XML are in `build-windows/checks-1a249ede11bf4c51b7f95f9ff65e0afe`.

The next chunk is the ceremony around Mankar, Ruma and the captive Jeelius. The stage 45 script calls `SetUnconscious` on Jeelius. The replay logged that operation as deferred, so the captive's pose and subsequent choices still need runtime acceptance.
