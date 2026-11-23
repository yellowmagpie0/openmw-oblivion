# City life performance on Windows

The November 2026 city-life change restored pedestrians in the Imperial City and guard relief in Chorrol. It also made the engine rebuild the same 382 native game settings more often as additional NPCs walked and regenerated fatigue. During an active Oblivion actor update, the engine now resolves settings for armor weight, frame resources and essential recovery on first use and shares them across actors. Inventory, equipped slots and skills are still read on every weight query. Each new pass reads the current winning records, including overrides.

## Measurements

Two 50-second samples per build and scene, after five seconds of gameplay warmup. Each process used the same fixed camera, seed, uncapped 1280×720 window and Oblivion master, with sound disabled. Hardware was an Intel Core Ultra 7 355 with Intel Graphics. FPS and percentiles use pooled gameplay frames. The Market starts at 13:00 and Chorrol at 16:00. The reload uses the same populated Market save at about 14:30.

| Scene | Before city life, FPS | With city life, FPS | Optimized, FPS | 95th-percentile frame time with city life → optimized |
| --- | ---: | ---: | ---: | ---: |
| Imperial City Market District | 29.8 | 24.0 | 35.4 | 48.7 → 33.5 ms |
| Chorrol guard relief | 36.2 | 23.7 | 36.0 | 51.0 → 33.6 ms |
| Populated Market save | 15.9 | 15.1 | 19.2 | 106.3 → 88.9 ms |

The optimized Market had 15–21 nearby NPCs, compared with 11–13 before city life. Chorrol went from 8 to as many as 23. In Chorrol, the earlier, sparsely populated version had a 31.1 ms 95th-percentile frame. The populated save still has long frames. These runs cover stationary gameplay on this machine.

Three separate Market runs contributed 400 main-thread stack samples per build. Inventory weight was present in 76 samples before city life, 95 with city life and 14 after optimization. Mean mechanics time in the unprofiled Market runs fell from 29.2 to 16.7 ms per frame and in Chorrol from 31.2 to 17.7 ms.

## Evidence

The controls are the parent revision `e14af5ed89` and the city-life implementation committed as `30296409b7`. The latter binary was built before the commit and carries the parent revision stamp. The executable hashes, per-trial FPS and percentiles, subsystem timings, actor counts and separate stack profiles are in `build-windows/city-perf/comparison.json`. The benchmark configuration, raw frames and logs are alongside it. The populated save also reports the same three missing textures on all builds.

All 3,322 component tests and 2,344 engine tests passed with exact test inventories and zero skips. The tested source fingerprint is `41473b375cbd22ae0407cd03e65d5df0b2fc147164c5108dff42afc420e01efb`. Full test evidence is in `build-windows/checks-f69e664e42b5417f921b16cdaf776572`. The added regression covers store identity, dynamic overrides, clearing the override and scope cleanup after invalid input.

Both in-game behavior checks passed. Gin-Wulm, Cicero Verus, Mandil and Simplicia were present together in the Market. In Chorrol, the guards met, the night guard left through the door, the day guard held the post facing the authored direction, and Honditar entered town. The scenarios checked reference identities and rejected duplicates. The results and event logs are in `build-windows/city-perf/behavior-market_arrivals` and `build-windows/city-perf/behavior-chorrol_relief`.
