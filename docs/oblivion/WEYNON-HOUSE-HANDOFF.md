# Weynon House and the missing Amulet

MQ04 stage 20 begins with Martin and Jauffre alive in Weynon Priory Chapel. Leave through the chapel door, cross the courtyard, and enter Weynon House. Go upstairs and through the two secret cabinet doors. Inspect the empty chest and finish Jauffre's conversation about Martin's safety. At stage 30 the journal directs the player to escort both men to Cloud Ruler Temple.

The earlier [Kvatch and chapel course](KVATCH-HANDOFF.md) supplies the starting save and its provenance. Continue with the [escort to Cloud Ruler Temple](CLOUD-RULER-GATES-HANDOFF.md).

## Replay and resume

From the checkout, use `build-windows/Play-Weynon-Reunion.cmd` to play from the chapel or `build-windows/Play-Weynon-House.cmd` to resume after the discovery. Both profiles use the original Oblivion data and the same saved content list. The named saves are separate from the ordinary quicksave.

For a repeatable run, invoke `python build-windows/weynon-inspection-run.py TAG build-windows/sigil-weynon-player/user/saves/Bendu_Olo/Martin-at-Weynon.omwsave course` with a new local tag. The input harness walks the navmesh and activates the doors, chest and dialogue through the game. `scripts/verify_weynon_investigation.py` checks the resulting save, including the quest script local, dialogue history, empty chest, door poses, both living companions and the temple marker.

## Why the course previously broke

- The chapel save predated persistence of an offscreen door's pending motion. On entering the house the cabinets could stay shut. Physical door motion now survives save/load independently of its scripted default. A legacy save with the open default, no animation and the original closed hinge position can resume the opening.
- Jauffre could stay in an obsolete sleep package after its route stalled. The resident and offscreen AI now reconsider an interruptible stalled package when its selection timer expires. A mandatory follow package retains its assignment. The player can reach the chest first (stage 25), or Jauffre can finish his travel package first (stage 27). His final reply is INFO `01ee2e`.
- A player standing at the opened chest could see Jauffre running in place against the left cabinet. His path had an approach point in front of the opening, but the package's 64-unit arrival radius let him turn toward the far side while still beside the frame. Intermediate waypoints now use the tighter steering tolerance. Once an actor opens a door, the obstruction ray excludes the same leaf as the movement sweep until the actor clears it. A new quest stage also prompts travelling actors to reconsider interruptible assignments, including when dialogue concludes before Jauffre reaches his inspection marker.
- The stage-30 result calls `ShowMap CloudRulerTempleMapMarker 1`. The original marker `0177ab` starts with both flags clear. In the local Oblivion 1.2.0416 executable, handler `0x50ad00` sets the visible bit and a nonzero argument also sets the travel bit. The two flag setters preserve existing bits. The executable SHA-256 is `a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`. The fork persists those bits by reference and uses them in the atlas and travel eligibility check.

## Local evidence

The rendered cases are under `build-windows`. `sigil-amulet-final01` records the route from the chapel, chest activation, Jauffre's stage result and the first earned save with the marker. In `sigil-amulet-map01/user/screenshots/screenshot001.png`, a fresh process opening that save offers travel to Cloud Ruler Temple. The run ended in the house with the travel prompt open.

`sigil-amulet-final02/openmw.log` records successful playback of six original Breton clips across the greeting and two replies. The male Breton voice uses its own race, encoded as zero in RACE/VNAM. Preserving that zero when the content index shifts lets the menu resolve the original files in Voices1.bsa. The final save has Martin following the player and Jauffre following Martin.

The performance controls are the walking-frame samples in `sigil-timed-before2/openmw.log`, collected at 1280 × 720 with a 30 FPS cap and sound enabled. Capture includes frames during traversal and excludes time spent paused in menus. The full native test run writes its exact inventories, XML results, executable hashes and source fingerprint to the directory named by `weynon-house-full-tests.log`.

On that machine, the chapel, courtyard and upstairs walks averaged 12.8, 14.9 and 23.6 FPS in the control. The corresponding voiced replay averaged 28.9, 21.4 and 29.0 FPS. The courtyard's 95th-percentile frame fell from 102 to 61 ms. A 632 ms frame remained in that cold-profile crossing. The runtime changes reduce repeated saved-reference and door-route searches. Each run used a fresh navigation cache.

`sigil-amulet-final03` repeats the course with the previous run's navigation cache and the revised actor lighting. The same walks averaged 28.9, 24.5 and 28.1 FPS. In the courtyard, the 95th-percentile frame took 58 ms and the slowest took 119 ms. The screenshots show the courtyard, the empty chest and Jauffre speaking by candlelight. Native dialogue temporarily hides the first-person weapon and objective banner, and the exterior sky uses the Oblivion sun and glare textures.

For the cabinet regression, `sigil-house03` is an earned player-first save at stage 25. With the player standing beside the chest, `sigil-cabinet-trace01` records Jauffre running against the frame at y=227. The same save in `sigil-cabinet-corner02` shows him cross the hinge line at y=257, walk up to the chest, stop at y=563, and walk back out after the conversation. The folder includes screenshots from both sides and an AI event log. Pass `--crossing-log build-windows/sigil-cabinet-corner02/openmw.log` to the save verifier to check his physical crossing and animation. The Windows game executable used for this replay has SHA-256 `b532d90e5a0b0b3552be12246d9940c35752cb467906ce74ab6a076a4f96d890`.

The final `sigil-amulet-verified` replay started at the chapel. Jauffre reached the secret room first, played all six recorded voice clips, and the player opened the empty chest. The chapel, courtyard and upstairs walks averaged 29.5, 26.0 and 28.2 FPS with a warm navigation cache. `sigil-amulet-resume` cold-loaded that save under ordinary player controls and saved again. Both saves passed the state verifier. `sigil-amulet-map-final/user/screenshots/screenshot001.png` shows the resulting fast-travel confirmation. The full Windows test inventories passed with 2,347 engine cases, 3,323 component cases and zero skips. The XML and tested source fingerprint are in `build-windows/checks-8ee2ee98fcea4d6fab351e86c31712ae`.

`sigil-cabinet-film/Jauffre-crossing.webp` records the crossing from the player's viewpoint. `sigil-cabinet-stuck-save-recovery` separately loaded the old stage-30 save in which Jauffre was still assigned to inspect the chest. Without player input he picked up his follow-Martin package and the resaved party passed verification.
