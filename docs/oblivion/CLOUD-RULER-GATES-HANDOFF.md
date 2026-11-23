# Weynon House to the gates of Cloud Ruler Temple

Start with the [Weynon House discovery](WEYNON-HOUSE-HANDOFF.md) at MQ04 stage 30. Leave the upstairs secret room, take the stairs, and go outside. Martin follows the player and Jauffre follows Martin. The overland course takes the roads toward Bruma and climbs the switchbacks to the main gate. The world map also offers Cloud Ruler Temple after Jauffre's conversation. Leave nearby enemies behind before travelling and walk up from the map marker with both companions.

The original MQ04 script advances to stage 35 when Martin is within 1,000 game units of the main gate and Jauffre is within 1,250. That stage unlocks the main gate, Great Hall and both wings. The next chunk begins with the reception at the gate.

## Play and verify

Use `build-windows/Play-Weynon-to-Cloud-Ruler.cmd` for the full trip or `build-windows/Play-Cloud-Ruler-Gates.cmd` for the arrival. Both use ordinary player controls, a 1,280 × 720 window, a 30 FPS cap and distant terrain with a viewing distance of 14,336 game units. The house save in `sigil-amulet-resume/checkpoints` has SHA-256 `782c8ba5f67523f3ce82e62cdf5bfe0078aba98bfd2b9175ad7e8b11b9a9dcd4`.

Run `python scripts/verify_cloud_ruler_arrival.py SAVE` to check both living actors, their distances, the player's position, the original stage progression and all four locks. Add `--road-log LOG` to check the house exit, both actors' physical positions along the road and sustained movement at each crossing. Use `--map-log LOG` to check separate landing positions after actual fast travel.

The local input harness is `python build-windows/temple-run.py TAG course` with a fresh tag. Its player script follows the original Tamriel ROAD points, activates the house door and fights the road encounter through player controls. The two companions keep their authored packages. Optional mode `map` uses the game's travel confirmation. Screenshots and milestone saves are in the corresponding `build-windows/temple-TAG` profile.

## Regression saves and evidence

- `temple-road04/border-stall.omwsave` captures Martin running back and forth where the player crossed the first exterior grid line. `temple-road05/jauffre-stall.omwsave` captures Jauffre holding a failed route to Martin's old position. Nearby followers can now route across a shared exterior navmesh and recover when their leader moves.
- `temple-road06/bruma-road-encounter.omwsave` precedes the fight. `temple-road07/bruma-turn.omwsave` captures Martin at the mountain rock. The sampled corridor passed close to the rock's collision surface. The resident follower now uses the engine's obstacle steering and replans after stepping around it. `temple-road10/gates.omwsave` finished this route at stage 35 with Martin 733 units and Jauffre 1,029 units from the main gate.
- `temple-map02/gates.omwsave` reached the same stage from the house by world map with both companions. Native map travel follows each active link in the party. Distant combat engagements had previously kept the peaceful road in battle music and could block the map after the player left the attackers behind.
- `temple-road04` records a screenshot hotkey and quicksave on the same frame. The thumbnail waits for its own framebuffer readback. `temple-resume01` and `temple-resume02` contain the pre-fix dumps from loading the gate with distant terrain enabled. The background cache worker still held a registration for destroyed terrain paging objects.
- `temple-road11/openmw.log` records one uninterrupted journey from the house to the reception. Through the stage-35 escort, the greatest sampled player distances were 621 units for Martin and 1,033 for Jauffre. Both were present at every road checkpoint. After stage 40 they walked up the temple stairs as the reception directed.
- `temple-resume03/gates.omwsave` is a cold load and second save with distant terrain enabled, using normal player controls. It retains all four unlocks and both living actors on the gate apron. Worldspace terrain managers now deregister before destruction, and a background cache update finishes before its manager can disappear.

## Delivered checkpoint

`temple-delivery-road` replayed the full overland course from the original upstairs save with the finished executable and player graphics settings. The verifier found both companions at the house exit and every road checkpoint. Their greatest sampled distances from the player were 964 units for Martin and 1,476 for Jauffre. The named checkpoint is a copy of its stage-35 save, SHA-256 `7941040a56f4583ba7d34cdd06c40a13be5ea51c7e6961c162fdc5f72ec20727`. Martin was 886 units from the gate and Jauffre 1,223. The fast-travel replay `temple-map04` also started upstairs, used the map's confirmation, and finished with both companions alive at stage 35. All three travellers occupied distinct positions in the first sample after landing.

The final executable has SHA-256 `cb959baf26c1fb2f6b8ddac6a161974167d1a6168225ef8ad9295d948d580904`. Both complete Windows test inventories passed, with 2,347 engine tests, 3,323 component tests and zero skips. The XML, binary hashes and tested source fingerprint are in `build-windows/checks-af040d3e213c4f1e89e3842b5ecd735b`.

`temple-map04/user/screenshots/screenshot000.png` shows the road and the working Tamriel minimap. The minimap requests tiles and door markers in the active worldspace and uses its exterior cell dimensions. `temple-player-ready/Cloud-Ruler-Gates.png` shows the two companions at the unlocked gate. The run log still reports a butterfly path-interpolator fallback and a missing Shivering Isles flame mesh while streaming the road.

## Frame timing

With the player settings, Intel integrated graphics and a warm navigation cache, `temple-delivery-road` recorded 16,621 frames over 739.5 seconds of gameplay, including combat and cell streaming. The average was 22.5 FPS, the median frame was 38.5 ms and the 95th percentile was 75.2 ms. The longest frame took 1.08 seconds, with 1.06 seconds in the mechanics update. The full course still has visible hitches. Active frame statistics exclude startup and the terminal quicksave.

The isolated Bruma encounter and climb in `temple-summit-final` averaged 25.8 FPS across 2,883 frames, with a 51.3 ms 95th percentile and a 319 ms maximum. `temple-gate-delivery` cold-loaded the delivered checkpoint and saved it again. The 14-second stationary sample averaged 27.5 FPS across 386 frames, with a 47.4 ms 95th percentile and a 60.2 ms maximum. Both companions settled into idle animations. The second save retained stage 35 and all four unlocks.
