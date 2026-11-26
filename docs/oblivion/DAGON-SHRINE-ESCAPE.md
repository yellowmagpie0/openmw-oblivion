# Escaping with the Mysterium Xarxes

Run `build-windows/Continue-After-Escaping-the-Shrine.cmd` to resume outside Lake Arrius. The player carries the Xarxes, Jeelius survived, and the active journal objective is to bring the book to Martin. The [previous checkpoint](DAGON-SHRINE-CEREMONY.md) starts with the cult hostile and the book still on its lectern.

## Playthrough

Open the Xarxes and take it from the reading window. The statue collapses and the upper gate opens. Cross into the living quarters and protect Jeelius on the way to the rear door. Turn the handle in the caverns to open the rock wall, then follow the passage to daylight. Jeelius uses the doors himself. One trial reached stage 70 with Jeelius dead in the living quarters, so the journal alone cannot confirm the rescue.

The playthrough used movement and camera input assistance with ordinary in-game interactions and saves. The living-quarters fight was replayed at reduced difficulty in an isolated profile. The cavern exit and playable handoff use the normal difficulty setting.

## Verification

The original master defines two 490 × 650 illustrations in the book markup, a page break and an inventory pickup script. The game renders the artwork from the native menu textures and fires the pickup event after adding the book. The event advances MQ06 to 60, triggers the shrine animations and clears the quest flag on the shrine key. The release and theft change Jeelius's package. Once the player escapes, his editor-location wander takes him back toward the Temple of the One.

The original `Oblivion.esm` SHA-256 is `a26e21ea8c3041f8737ffb3a266129dedb7f8a88590625ecfecd5eb7f66b4a70`. The starting save SHA-256 is `0c9a4620ceb465a7875769758057ce7edc54ca81e5c84543288935a3a52aa599`. Local evidence is under `build-windows`. The illustrated spread is in `sponsor-xarxes09/Book.png`. The pickup and first door are in `sponsor-xarxes07/openmw.log`. The rear door, handle, exit and stage 70 are in `sponsor-xarxes13/openmw.log`.

With the 30 FPS cap at 1280 × 720, the cavern sample averaged 29.5 FPS with a 37.7 ms 95th-percentile frame. The exterior sample averaged 28.9 FPS with a 42.5 ms 95th-percentile frame. Neither sample had a frame over 100 ms. These are gameplay intervals in `sponsor-xarxes13/stats.txt`, at OSG reference times 49–67 and 91–121 seconds respectively. They exclude the exterior loading transition.

Jeelius walked over 300 game units from the exterior landing in `sponsor-xarxes17`. A fresh process with stock controls reloaded the checkpoint and saved him another 280 game units along the route. The screenshot `sponsor-xarxes-stock01/Checkpoint.png` shows him walking downhill. The playable checkpoint SHA-256 is `db82e51b2a1a75e41abd1a752071d8f2424869e933194ff6add8717830adff84`. The engine and component inventories passed with 2,363 and 3,330 tests in `checks-214d5b418ecb40e8ab692598006adb5a`. All 212 portable Python checks passed.

Run the saved-state and ordered gameplay checks from the repository root. The restart save is taken after Jeelius has walked at least 80 game units from his checkpoint position.

```powershell
$checkpoint = 'build-windows/temple-player-ready/checkpoints/Dagon-Shrine-Escaped.omwsave'
python scripts/verify_shrine_escape.py $checkpoint --restart-save build-windows/sponsor-xarxes-stock01/Reloaded.omwsave --take-log build-windows/sponsor-xarxes07/openmw.log --escape-log build-windows/sponsor-xarxes13/openmw.log
```
