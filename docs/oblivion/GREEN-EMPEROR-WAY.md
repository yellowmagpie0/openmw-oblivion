# The noon inscription on Green Emperor Way

From [Tar-Meena's cipher](TAR-MEENAS-CIPHER.md), leave the Arch-Mage's Lobby, travel to the Imperial City Palace and approach the inscribed face of the Tomb of Prince Camarril. Read it while the red map is visible around noon. This finishes The Path of Dawn and starts Dagon Shrine. Continue with [the journey to Lake Arrius and the doorkeeper's password](LAKE-ARRIUS-CAVERNS.md).

The winning Oblivion master (SHA-256 `a26e21ea8c3041f8737ffb3a266129dedb7f8a88590625ecfecd5eb7f66b4a70`) places tomb `024e4a` with base `06a5ca` and mesh `Architecture/ImperialCity/ICMausoleumMQ01.NIF`. Script `024dcb` plays the mesh's forward group after 11:48 through 12:30 and reverses it after 12:30. Activation requires a time strictly between 11:48 and 12:30. Merely waiting beside the glowing stone leaves the quest at stage 93.

The stage 95 result goes directly to 100. With all four volumes, entry 1 releases Volume IV, stops The Path of Dawn, starts Dagon Shrine at stage 10 and awards one Fame. The new quest reveals Lake Arrius, enables Harrow and Mankar Camoran, gives the doorkeeper the antechamber key and rearms living Baurus. Volume IV can be removed and weighs five units. For the underlying `SetQuestObject` command, the original executable (SHA-256 `a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`) dispatches opcode `0x110D` at `0x5052D0` and tests whether the signed flag is greater than zero.

Volume III is still protected in this checkpoint. When Gwinas handed it over on an earlier build, the engine deferred the book script's command to release it (`build-windows/bookshop-delivery12/openmw.log`). The current implementation handles that command on acquisition. The tomb releases only Volume IV.

The game stores book flag overrides by native base in save format 110, and retains script-revealed landmarks in unloaded cells. The compass can route between separate exterior districts using their teleport gates. Dropping and taking the released Volume IV transfers its script between the inventory stack and the loose reference. On pickup the script requests stage 90 again. The original SetStage handler at `0x529AEF` checks completion and the repeatable-stage flag before restarting a stopped quest.

The master declares GameHour (`000038`) as a short. The original expression evaluator's global operand at `0x4FAC03` reads the float at form offset `0x24` directly. GetCurrentTime reads the same field via `0x4F46E0` and `0x402BD0`. ObScript now reads the fractional clock. On the replay the inscription appeared at 11:48, and the player read it before 12:00. Previously it first appeared at 12:00.

## Playable checkpoint

Run `build-windows/Continue-After-the-Noon-Inscription.cmd` to resume facing the inscription. The checkpoint is `build-windows/temple-player-ready/checkpoints/Dagon-Shrine-Lake-Arrius-Lead.omwsave` (SHA-256 `f87b19622718e8b46ed219f5c36d3b0e00eb13cc887b14de4e5d5c6d7442c019`).

The full replay is in `build-windows/sponsor-tomb-final05` on game SHA-256 `9affac2c38f86a2f2834c3020076eb0a4f372e3ec3e316c55e097016a5d4dda4`. A movement and camera aid walked the player to the door and tomb and opened the map. The player used the lobby door, the Palace fast-travel confirmation, the Wait menu and normal activation. Afternoon activation left The Path of Dawn at stage 93. `Afternoon.png`, `Noon-inscription.png` and `Dagon-Shrine.png` show the same stone before illumination, while the map is visible and after the quest change. The final compass points toward a city gate, 24 metres away. In a stock-control restart (`build-windows/sponsor-tomb-stock07`), `Returned-to-Stone.png` shows the relief after it faded and `Lake-Arrius-landmark.png` shows the visible, unvisited destination on the world map.

At the same tomb viewpoint at 1280 × 720 and a 30 FPS cap, a 25-second sample after 15 seconds of warm-up rose from 22.1 FPS to 27.9 FPS. The 95th-percentile frame time fell from 60.0 to 45.8 ms and frames over 50 ms fell from 107 to 13. The main-thread profile pointed to ordinary NPC inventory actions searching loaded cells for activators. Checking the action target's base type first avoids that search for food and equipment. The samples are in `build-windows/sponsor-tomb-stock07` and `build-windows/sponsor-tomb-stock08`.

On the final game executable (SHA-256 `11274426584df08597c1f4f2feda3bc7bb99a19b9dc1760f7765843149deadf0`), the player reopened the dropped Volume IV and used Take. The resave contains one carried copy and one inventory script owner. The Path of Dawn is still stopped at 100 and Dagon Shrine is active at 10. A separate stock-controls restart of the playable checkpoint retained the glowing map and city-gate compass. Its screenshot and resave are in `build-windows/sponsor-tomb-stock11`.

All 2,359 engine tests, 3,328 component tests and 212 portable Python tests passed on Windows. The C++ inventory, XML and source fingerprint are in `build-windows/checks-ce0bc1b0dbd64143b4680386645c42f2`. The broader Python discovery run also includes Linux-specific tooling and failed on this host, including the HUD fixture's ImageMagick dependency.

```powershell
$course = "build-windows/sponsor-tomb-final05"
python scripts/verify_noon_inscription.py "$course/Lake-Arrius-lead.omwsave" --course-log "$course/openmw.log" --approach "$course/Afternoon.omwsave" "$course/Glowing.omwsave" --restart-save build-windows/sponsor-tomb-stock11/Reloaded.omwsave --book-round-trip build-windows/sponsor-tomb-stock08/Dropped.omwsave build-windows/sponsor-tomb-pick10/Recovered.omwsave
```
