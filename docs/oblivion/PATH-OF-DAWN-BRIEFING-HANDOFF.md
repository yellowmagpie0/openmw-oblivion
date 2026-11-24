# The Path of Dawn at Cloud Ruler Temple

The course begins at the [courtyard checkpoint](CLOUD-RULER-RECEPTION-HANDOFF.md). Finish Martin’s conversation, approach Jauffre, choose “No, not right now” when he offers Blades membership, then select “Amulet of Kings”. MQ04 finishes at stage 100 and MQ05 reaches stage 20. Jauffre directs the player to Baurus at Luther Broad’s Boarding House in the Imperial City’s Elven Gardens District. Blades membership remains available.

## Original quest results

In Oblivion.esm (`a26e21ea8c3041f8737ffb3a266129dedb7f8a88590625ecfecd5eb7f66b4a70`), MQ04 stage 50 sets stage 100, then grants Martin’s Emperor faction rank. Stage 100 renames him, resets the Mythic Dawn reaction toward the Blades and restores his aggression. MQ05 stage 20 enables Astav Wirich, removes Baurus’s Blades armour, adjusts his factions, sets his level to the player’s level plus one, sets aggression to five, moves him to marker `03011b` and enables friendly-hit protection. The next conversation requires the seated player to be near Baurus.

The original executable (`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`) resolves the actor base in the SetActorFullName handler at `00512BB0`. SetLevel at `0050CD30` writes the signed level word and updates the offset flag and limits when requested. The engine stores both mutations on the shared actor base. The current save schema is 108. A version-107 courtyard save is the starting fixture.

## Acceptance

`scripts/verify_path_of_dawn_briefing.py SAVE --log LOG` checks the order of both conversations, the approach to Jauffre, the journal and final script effects. It also checks Baurus’s placed reference and AI cell, the meeting marker, and Astav’s enabled state. Run it without `--log` to verify a separately reloaded and resaved checkpoint.

The final run, `build-windows/dawn-briefing07`, loaded the courtyard checkpoint, moved the player across the courtyard and used the conversation choices and topic list. The briefing used three voiced responses with corresponding lip files. Over a 60-second sample covering dialogue and the approach, 1,727 frames averaged 28.8 FPS at 1280 × 720 with a 30 FPS cap. The 95th-percentile frame took 35.2 ms.

Launch `build-windows/Continue-After-Jauffres-Briefing.cmd` to resume at the temple. The delivered checkpoint is `build-windows/temple-player-ready/checkpoints/Path-of-Dawn-Briefing.omwsave`, SHA-256 `8875a13049fa4a64e4975b5af99071367f06d8f1be252acec8af7f4eccbb95c5`. A stock-control reload displayed the courtyard and the Baurus objective. A separate inspection run, `dawn-inn07`, first reloaded and resaved the checkpoint. The console then moved only the player to the boarding house, and the player walked to Baurus. His live level was 2 for a level-1 player. The HUD identified him at one metre. Screenshots of Jauffre and Baurus are in `temple-player-ready`.

The game executable has SHA-256 `3e4e7f7b5099153f546d96bbb52586d3e5263887f4f210c446cc158d1a9ed7dc`. All 2,347 engine tests, 3,324 component tests and 205 runtime-state Python tests passed. The C++ inventories, XML and source fingerprint are in `build-windows/checks-141d80eb8d484fa190d6d891a028adb4`.

Baurus was standing at the counter with his sword drawn during inspection. The next chunk is his seated meeting, the player chair trigger and the encounter with his tail in the basement.
