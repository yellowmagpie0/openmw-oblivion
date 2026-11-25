# Astav’s ambush at Luther Broad’s

Start from the standing save after [Baurus’s warning](LUTHER-BROADS-SURVEILLANCE-HANDOFF.md). Follow Astav through the inn’s basement door and down the stairs. He equips Mythic Dawn armor and a mace when he attacks Baurus. After the fight, hear Baurus out, search Astav, take *Mythic Dawn Commentaries 1*, then speak to Baurus again. Tell him the enemy has the Amulet and ask what to do next. The next objective is to speak to Tar-Meena at the Arcane University.

## Original records and runtime checks

In the installed Oblivion.esm (SHA-256 `a26e21ea8c3041f8737ffb3a266129dedb7f8a88590625ecfecd5eb7f66b4a70`), MQ05 stage 31 starts Astav’s combat against Baurus. Astav carries the zero-cost lesser power `022b37`. Its three self effects each last 1,000 seconds. The engine equips this unambiguous bound set on combat entry, saves its remaining lifetime, and restores displaced gear on expiry or death. This covers the equipment reveal. General NPC spell choice and cast choreography remain separate work.

Astav’s death advances stage 32. Baurus’s search instruction advances stage 34. The book’s own `OnDrop MQ05AstavWirichRef` handler advances stage 35 when the player loots it. Baurus’s final three-part reply advances stage 36. The authored objective now names Tar-Meena’s placed reference `034ebc`.

Run `python scripts/verify_luther_broads_basement.py SAVE --fight-save COMBAT_SAVE --log OPENMW_LOG` to check the full course. Run it with just a save path after a fresh stock-control restart. The checks cover actor life and location, equipped items during the fight, death cleanup, book ownership, journal history and the last voiced line. Inspect the in-game capture separately for the compass, models and poses.

The local launchers are `build-windows/Continue-After-Baurus-Warning.cmd` for the start and `build-windows/Continue-After-Baurus-Basement.cmd` for the handoff. The earlier trapped-stair reproduction is in `build-windows/basement-route02`. The stationary replay in `build-windows/basement-stairs07` reaches Astav’s death and Baurus’s search instruction without moving the player.

The frame trace in `build-windows/basement-final08` exposed two pauses while granting and removing the bound equipment. Each operation captured the entire world. The revised transaction stages the affected actor’s inventory and ownership, then publishes both before notifying equipment observers. The repeat course and frame samples are in `build-windows/basement-timed12`. A separate stock-control sample in `build-windows/basement-perf11` showed intermittent mechanics stalls after the conversation. That sample averaged 21.8 FPS over its final 30 seconds at 1280 × 720 with a 30 FPS cap.

Continue with [Tar-Meena’s briefing at the Arcane University](ARCANE-UNIVERSITY-TAR-MEENA-HANDOFF.md).
