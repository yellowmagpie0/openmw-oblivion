# Giving Azura’s Star to Martin

Run `build-windows/Continue-After-Giving-Martin-Azuras-Star.cmd` to resume in the Great Hall. Martin has accepted the artifact for the ritual. *Bruma Gate* directs the Knight to speak to Jauffre. The [shrine checkpoint](AZURAS-STAR.md) still holds the Star for replaying the choice.

Fast travel at the shrine initially reported enemies nearby. A live wolf on the shrine road was engaged with the player. The Knight went east on foot, used the world map to travel to Cloud Ruler Temple and climbed the central stairs to the Great Hall. Martin was studying at the table. His *Daedric artifact* topic offered two choices. Giving him the Star removed one copy, completed *Blood of the Daedra* and disabled the quest wisps. Asking what else he needed started *Bruma Gate*.

The first conversation showed a broad white reflection across Martin’s eyes. An isolated replay with the eye meshes’ specular color set to zero identified the source. The renderer now narrows and dims the corneal reflection on the actor’s own eye material. The final replay uses the original game meshes. Compare `build-windows/sponsor-starmartin01/Talk.png` with `build-windows/sponsor-starmartin02/Greeting.png`.

The route from the shrine and the pre-conversation save are in `build-windows/sponsor-starmartin01`. The final handoff is in `build-windows/sponsor-starmartin02`. The stock-controls reload, screenshot and frame timings are in `build-windows/sponsor-starmartin-stock01`.

```powershell
$saves = 'build-windows/temple-player-ready/checkpoints'
$trip = 'build-windows/sponsor-starmartin01'
python scripts/verify_martin_artifact.py "$saves/Azuras-Star-Received.omwsave" "$trip/BeforeTalk.omwsave" "$saves/Azuras-Star-Delivered.omwsave" build-windows/sponsor-starmartin-stock01/Reloaded.omwsave "$trip/openmw.log" build-windows/sponsor-starmartin02/openmw.log
```
