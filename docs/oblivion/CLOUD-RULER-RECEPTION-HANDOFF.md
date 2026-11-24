# Cloud Ruler Temple reception

The [Path of Dawn briefing](PATH-OF-DAWN-BRIEFING-HANDOFF.md) continues from the courtyard save through Jauffre’s lead on Baurus.

Start at the [earned gate checkpoint](CLOUD-RULER-GATES-HANDOFF.md). Walk up to the gate with Martin and Jauffre. Cyrus comes down to speak to Jauffre and welcome Martin. Go through the gate and follow them up the stairs. The stopping point is MQ04 stage 45, after Jauffre dismisses the Blades. The next interaction is Martin’s conversation with the player about the Amulet of Kings.

## Original scene

The winning Oblivion.esm used here has SHA-256 `a26e21ea8c3041f8737ffb3a266129dedb7f8a88590625ecfecd5eb7f66b4a70`. The five waiting packages become eligible at MQ04 stage 30. Steffan, Jena and Belisarius occupy the western posts. Caroline and Roliand take the eastern posts. Cyrus greets the party and then takes the remaining eastern post. All six markers are XMarkerHeading references. The stage-40 script asks the five waiting Blades to look at Martin as he comes through. Cyrus looks at him at stage 41.

After Jauffre’s introduction, Cyrus calls out and stage 42 starts the five answering voices together. Each guard’s UseItemAt package selects IDLE `066393`, which plays `OneHandCheer.kf`. Stage 43 lowers their alert state and directs all six to look at Martin. Stage 44 releases the formation. The HUD names Cloud Ruler Temple while the journal keeps the authored quest title, Weynon Priory.

## Local replay and checks

`build-windows/Watch-Cloud-Ruler-Reception.cmd` walks the player through the scene and returns control after dismissal. NPC movement, dialogue, animation and stage changes come from the engine. `build-windows/Play-Cloud-Ruler-Reception.cmd` starts at the same gate with ordinary controls. `build-windows/Continue-After-Cloud-Ruler-Reception.cmd` starts at the courtyard checkpoint. The original gate save is still available.

Run `python scripts/verify_cloud_ruler_reception.py SAVE --log LOG` to check the voiced sequence, physical player movement, six posts before Martin enters the ranks, six salutes, the audience’s orientation, dismissal and the saved quest state. The local record audit, screenshots, logs, mid-greeting save and frame statistics are under `build-windows`. The original records establish the authored sequence. The rendered course verifies this fork’s playback.

The earlier gate checkpoint starts Cyrus inside the temple with a 30-second quest timer already running. In `temple-reception08` he reached Jauffre in time and played both sides of their exchange. A new process loaded a save made during his first line in `temple-reception-resume01`, played Jauffre’s reply and finished the reception. In `temple-reception09` Cyrus reached the gate just before the timer expired and the authored fallback sent Martin up the steps before Jauffre’s reply. All six guards still saluted. Starting on the approach gives the stage-30 packages time to assemble the party.

`temple-reception09/user/screenshots/screenshot001.png` shows the six Blades in armour with their swords raised and Martin at the front with his weapon sheathed. During the climb and courtyard sequence, renderer reference seconds 80–162 averaged 25.2 FPS across 2,066 frames at 1280 × 720 on Intel integrated graphics. The 95th-percentile frame took 53.3 ms. The profile caps the frame rate at 30 FPS.

## Delivered checkpoints

`temple-reception-approach02` resumed the earned road save and walked the party to the gate. The arrival save has SHA-256 `a1f70dade3d3f31c5fc341075ca9cfee5e9432a02ac0ff72a1a599f504967d6e`. Cyrus was already outside at his assigned post. `temple-reception10` resumed there and passed the course verifier through stage 45. The courtyard save has SHA-256 `5e8516d32c13a0f470105aa360a41996e0c34348b0c00d4c45d4ffe8985ed4ad`. Both files are in `build-windows/temple-player-ready/checkpoints`. The same profile contains the recorded `Cloud-Ruler-Salute.gif`.

`temple-watch-restart10` resumed midway through Cyrus’s greeting, played Jauffre’s reply, reached stage 45 and returned player control. A separate game using ordinary controls loaded the delivered courtyard save and displayed Martin’s conversation about recovering the Amulet. The exterior still logs the previously observed butterfly mesh fallback.

The game executable used for delivery has SHA-256 `26ff9f376107c39c61e9d81c6315e11f88f8abf8dc1e11db372eb837941c55e2`. All 2,347 engine tests and 3,324 component tests passed with zero skips. Inventories, XML and binary hashes are in `build-windows/checks-4f9844e418704343bb78bdab759e633a`. The tested source fingerprint is `07b1167f49d8fbdfa34719e39266a69f5feb8df58fb5a6cea57c0b2a6deb51b2`, before this handoff was updated with results.
