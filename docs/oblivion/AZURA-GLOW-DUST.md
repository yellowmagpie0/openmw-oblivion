# Glow dust for Azura

From [Azura’s Shrine](AZURAS-SHRINE-ARRIVAL.md), fast travel to Castle Bruma and walk through town to the Mages Guild. On this save the trip ends late at night. Wait until morning and speak to Jeanne Frasoric. Choose **Join the Mages Guild**, then accept. Jeanne gives the player the guild key and charter.

Take the west stairs down to the basement. Go through the door into the ingredient room on the east side of the central corridor. The loose glow dust is on the table beneath the green guild banner. The player can take it as a guild member and sleep in the basement bedroom. The continuation save is back upstairs in the guild hall.

## Continue playing

Run `build-windows/Continue-With-Glow-Dust.cmd`. The level-one Knight has the offering and has slept for an hour. The save records five of the ten major-skill increases needed for level 2. Earn five more, then sleep before returning to Azura’s Shrine at dawn or dusk. *Blood of the Daedra* remains at stage 15.

The [level-up and offering](AZURAS-OFFERING.md) continue from this checkpoint.

## Verification

The original PC executable (1.2.0416) implements `IsXbox` by returning zero. Jeanne’s enrollment result, `Oblivion.esm:02D286`, checks this command after recording the guild branch and before awarding an Xbox achievement. The first run reported an unsupported command at this point. The replay on the rebuilt executable played all three enrollment responses and completed the result script. The disassembly is in `build-windows/dust-platform-oracle.txt`.

The normal-input replay in `build-windows/sponsor-dust02` starts at the shrine. Its log records both loading doors, the ingredient pickup and the completed hour of sleep. The verifier checks that the player retains one sample, the world copy is gone, the guild membership and items persist, and the trip adds no crime. It repeats the save checks on a fresh launch with stock controls.

On a stock reload facing the guild counter, a 20-second sample recorded 580 frames at 28.9 FPS with a 30 FPS cap at 1280×720. The 95th-percentile frame time was 42.1 ms.

All 2,366 engine tests passed and matched the inventory in `build-windows/checks-e8fda3ad923f4e5caeb3fad3824b4f33`. The tested Windows executable has SHA-256 `f804bc391abfd34f3cf6d5f64498b7f647a792f8f134c7cd1f011a5260d44d2b`. The continuation save has SHA-256 `dc0f6da085e9e87c7d6d889ff820db7cb7536c10019f2d3a16deeb21b1c311bd`.

```powershell
$saves = 'build-windows/temple-player-ready/checkpoints'
python scripts/verify_azura_glow_dust.py "$saves/Blood-of-the-Daedra-At-Azuras-Shrine.omwsave" "$saves/Blood-of-the-Daedra-Glow-Dust.omwsave" build-windows/sponsor-dust-stock01/Reloaded.omwsave build-windows/sponsor-dust02/openmw.log
```
